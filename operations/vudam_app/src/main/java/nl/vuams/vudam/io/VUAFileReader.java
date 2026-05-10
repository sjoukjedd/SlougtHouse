package nl.vuams.vudam.io;

import nl.vuams.vudam.model.DataBlock;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

/**
 * Reads a binary .vua file and deserialises each block into a {@link DataBlock}.
 *
 * <h2>File format — v2 layout (with subject metadata header)</h2>
 *
 * Files written by firmware v2+ begin with a {@code subjectMetadata} section
 * before the first data block. The magic bytes {@code 0x56 0x55 0x4D 0x53}
 * ("VUMS") identify this header. Legacy files (no magic) start directly with
 * the first data block (type byte will be one of 'A','I','M','P','S','T').
 *
 * <pre>
 * Subject metadata header (variable length):
 *  Offset  Len        Type       Field
 *  0       4          uint8[4]   magic = { 0x56, 0x55, 0x4D, 0x53 } ("VUMS")
 *  4       2          uint16LE   header_len  — total byte length of this metadata section
 *                                             (including magic and header_len fields)
 *  6       4          float32LE  electrode_distance_cm — inner ICG electrode spacing (cm)
 *  10      2          uint16LE   subject_id_len        — byte length of subject_id UTF-8 string
 *  12      subject_id_len  UTF-8  subject_id
 *  12+subject_id_len  2  uint16LE  recording_date_len
 *  ...     recording_date_len  UTF-8  recording_date (ISO-8601, e.g. "2026-05-09")
 * </pre>
 *
 * After the metadata section (or at offset 0 for legacy files), the file is a
 * flat sequence of variable-length data blocks. Each block begins with the
 * 16-byte {@code vuams_block_header_t} (all fields little-endian):
 *
 * <pre>
 *  Offset  Len  Type      Field
 *  0       1    uint8     type        — BLOCK_TYPE_* ('A','I','M','P','S','T')
 *  1       1    uint8     version     — BLOCK_VERSION (0x01)
 *  2       2    uint16LE  payload_len — bytes of payload that follow the header
 *  4       8    uint64LE  timestamp_us
 *  12      4    uint8[4]  reserved / struct-alignment padding
 * </pre>
 *
 * Immediately after the header comes {@code payload_len} bytes of block-specific
 * data, also little-endian (C packed structs, no padding within payload).
 *
 * <h2>Block payloads</h2>
 * <ul>
 *   <li>A — 2000 bytes : int32[250] ecg1, int32[250] ecg2</li>
 *   <li>B — 8 bytes    : float baro_pressure_pa, float baro_temp_c</li>
 *   <li>I — 24 bytes   : float z0, dZdt_peak, pep_ms, lvet_ms, co_lpm, sv_ml</li>
 *   <li>M — 180 bytes  : int16[10] ax,ay,az,gx,gy,gz,mx,my,mz</li>
 *   <li>P — 14 bytes   : uint32 ppg_red, uint32 ppg_ir, float spo2, uint8 hr, uint8 valid</li>
 *   <li>S — 9 bytes    : float scl_tonic, float scl_phasic, uint8 contact</li>
 *   <li>T — 6 bytes    : float skin_temp_c, int16 temp_raw</li>
 *   <li>V — 606 bytes  : uint16 seq, uint32 timestamp_us, int16[100] ax, ay, az, uint16 crc</li>
 * </ul>
 */
public class VUAFileReader {

    /** Size of the common block header in bytes. */
    public static final int HEADER_SIZE = 16;

    /** Magic bytes identifying a v2+ file with a subject metadata header. */
    public static final byte[] METADATA_MAGIC = { 0x56, 0x55, 0x4D, 0x53 }; // "VUMS"

    /**
     * Subject metadata parsed from the optional file header.
     *
     * <p>{@code electrodDistanceCm} is required for the Kubicek stroke volume formula
     * (parameter L). It must be entered by the operator at session setup and is stored
     * here so that offline SV computation in VU-DAMS does not need manual re-entry.
     *
     * @param electrodeDistanceCm  distance between inner ICG voltage-sensing electrodes [cm]
     * @param subjectId            arbitrary subject identifier string (may be empty)
     * @param recordingDate        ISO-8601 date string, e.g. {@code "2026-05-09"} (may be empty)
     */
    public record SubjectMetadata(
            float electrodeDistanceCm,
            String subjectId,
            String recordingDate) {

        /** Sentinel returned when reading a legacy file that has no metadata header. */
        public static final SubjectMetadata ABSENT =
                new SubjectMetadata(Float.NaN, "", "");

        /** Returns true when electrode distance is present and physically plausible (5–50 cm). */
        public boolean hasValidElectrodeDistance() {
            return !Float.isNaN(electrodeDistanceCm)
                    && electrodeDistanceCm >= 5.0f
                    && electrodeDistanceCm <= 50.0f;
        }
    }

    /** Subject metadata read from the most-recently-parsed file, or {@link SubjectMetadata#ABSENT}. */
    private SubjectMetadata lastSubjectMetadata = SubjectMetadata.ABSENT;

    /** Returns the subject metadata from the last {@link #read(Path)} call. */
    public SubjectMetadata getLastSubjectMetadata() {
        return lastSubjectMetadata;
    }

    /**
     * Parses all blocks from the given .vua file path.
     *
     * <p>If the file begins with a subject metadata header (magic {@code "VUMS"}), it is
     * consumed and stored; retrieve it via {@link #getLastSubjectMetadata()} after this call.
     * Legacy files without the header are handled transparently.
     *
     * @param path path to the .vua file
     * @return ordered list of deserialized {@link DataBlock} instances
     * @throws IOException on any I/O or parse error
     */
    public List<DataBlock> read(Path path) throws IOException {
        lastSubjectMetadata = SubjectMetadata.ABSENT;

        List<DataBlock> blocks = new ArrayList<>();
        try (InputStream raw = Files.newInputStream(path);
             BufferedInputStream buf = new BufferedInputStream(raw, 65536);
             DataInputStream in = new DataInputStream(buf)) {

            // --- Detect and consume optional subject metadata header ---
            buf.mark(4);
            byte[] probe = new byte[4];
            int probeRead = 0;
            try {
                in.readFully(probe);
                probeRead = 4;
            } catch (EOFException e) {
                // file too short to have any content — fall through to empty block loop
            }
            if (probeRead == 4 && isMagic(probe)) {
                lastSubjectMetadata = parseSubjectMetadata(in);
            } else {
                // Legacy file: push back the 4 bytes we just read.
                // BufferedInputStream.mark/reset lets us do this cleanly.
                buf.reset();
            }

            while (true) {
                // --- Read header (16 bytes) ---
                byte[] headerBytes = new byte[HEADER_SIZE];
                try {
                    in.readFully(headerBytes);
                } catch (EOFException e) {
                    break; // clean end of file
                }

                ByteBuffer hdr = ByteBuffer.wrap(headerBytes).order(ByteOrder.LITTLE_ENDIAN);
                byte type       = hdr.get();       // offset 0
                byte version    = hdr.get();       // offset 1
                int payloadLen  = hdr.getShort() & 0xFFFF; // offset 2, unsigned
                long tsUs       = hdr.getLong();   // offset 4
                // 4 reserved bytes at offset 12 consumed by headerBytes

                if (version != 0x01) {
                    throw new IOException(
                            "Unsupported block version 0x%02X at block type '%c'"
                                    .formatted(version & 0xFF, (char) (type & 0xFF)));
                }

                // --- Read payload ---
                byte[] payload = new byte[payloadLen];
                in.readFully(payload);
                ByteBuffer p = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);

                DataBlock block = switch (type) {
                    case DataBlock.ABlock.TYPE -> parseABlock(type, version, tsUs, p, payloadLen);
                    case DataBlock.BBlock.TYPE -> parseBBlock(type, version, tsUs, p, payloadLen);
                    case DataBlock.IBlock.TYPE -> parseIBlock(type, version, tsUs, p, payloadLen);
                    case DataBlock.MBlock.TYPE -> parseMBlock(type, version, tsUs, p, payloadLen);
                    case DataBlock.PBlock.TYPE -> parsePBlock(type, version, tsUs, p, payloadLen);
                    case DataBlock.SBlock.TYPE -> parseSBlock(type, version, tsUs, p, payloadLen);
                    case DataBlock.TBlock.TYPE -> parseTBlock(type, version, tsUs, p, payloadLen);
                    case DataBlock.VBlock.TYPE -> parseVBlock(type, version, tsUs, p, payloadLen);
                    default -> throw new IOException(
                            "Unknown block type 0x%02X".formatted(type & 0xFF));
                };

                blocks.add(block);
            }
        }
        return blocks;
    }

    // -------------------------------------------------------------------------
    // A-block — 2000 bytes payload
    // -------------------------------------------------------------------------

    private DataBlock.ABlock parseABlock(byte type, byte version, long tsUs,
                                         ByteBuffer p, int payloadLen) throws IOException {
        final int expected = 2 * 250 * 4; // 2000
        requirePayloadLen(type, payloadLen, expected);

        int[] ecg1 = new int[250];
        int[] ecg2 = new int[250];
        for (int i = 0; i < 250; i++) ecg1[i] = p.getInt();
        for (int i = 0; i < 250; i++) ecg2[i] = p.getInt();

        return new DataBlock.ABlock(type, version, tsUs, ecg1, ecg2);
    }

    // -------------------------------------------------------------------------
    // I-block — 24 bytes payload
    // -------------------------------------------------------------------------

    private DataBlock.IBlock parseIBlock(byte type, byte version, long tsUs,
                                         ByteBuffer p, int payloadLen) throws IOException {
        requirePayloadLen(type, payloadLen, 24);

        float z0       = p.getFloat();
        float dZdt     = p.getFloat();
        float pepMs    = p.getFloat();
        float lvetMs   = p.getFloat();
        float coLpm    = p.getFloat();
        float svMl     = p.getFloat();

        return new DataBlock.IBlock(type, version, tsUs, z0, dZdt, pepMs, lvetMs, coLpm, svMl);
    }

    // -------------------------------------------------------------------------
    // M-block — 180 bytes payload
    // -------------------------------------------------------------------------

    private DataBlock.MBlock parseMBlock(byte type, byte version, long tsUs,
                                         ByteBuffer p, int payloadLen) throws IOException {
        requirePayloadLen(type, payloadLen, 180);

        short[] ax = readShorts(p, 10);
        short[] ay = readShorts(p, 10);
        short[] az = readShorts(p, 10);
        short[] gx = readShorts(p, 10);
        short[] gy = readShorts(p, 10);
        short[] gz = readShorts(p, 10);
        short[] mx = readShorts(p, 10);
        short[] my = readShorts(p, 10);
        short[] mz = readShorts(p, 10);

        return new DataBlock.MBlock(type, version, tsUs, ax, ay, az, gx, gy, gz, mx, my, mz);
    }

    // -------------------------------------------------------------------------
    // P-block — 14 bytes payload
    // -------------------------------------------------------------------------

    private DataBlock.PBlock parsePBlock(byte type, byte version, long tsUs,
                                         ByteBuffer p, int payloadLen) throws IOException {
        requirePayloadLen(type, payloadLen, 14);

        int ppgRed    = p.getInt();           // uint32
        int ppgIr     = p.getInt();           // uint32
        float spo2    = p.getFloat();
        int hr        = p.get() & 0xFF;       // uint8
        boolean valid = p.get() != 0;         // uint8

        return new DataBlock.PBlock(type, version, tsUs, ppgRed, ppgIr, spo2, hr, valid);
    }

    // -------------------------------------------------------------------------
    // S-block — 9 bytes payload
    // -------------------------------------------------------------------------

    private DataBlock.SBlock parseSBlock(byte type, byte version, long tsUs,
                                         ByteBuffer p, int payloadLen) throws IOException {
        requirePayloadLen(type, payloadLen, 9);

        float tonic   = p.getFloat();
        float phasic  = p.getFloat();
        boolean contact = p.get() != 0;

        return new DataBlock.SBlock(type, version, tsUs, tonic, phasic, contact);
    }

    // -------------------------------------------------------------------------
    // T-block — 6 bytes payload
    // -------------------------------------------------------------------------

    private DataBlock.TBlock parseTBlock(byte type, byte version, long tsUs,
                                         ByteBuffer p, int payloadLen) throws IOException {
        requirePayloadLen(type, payloadLen, 6);

        float tempC  = p.getFloat();
        short raw    = p.getShort();

        return new DataBlock.TBlock(type, version, tsUs, tempC, raw);
    }

    // -------------------------------------------------------------------------
    // B-block — 8 bytes payload (two floats; seq is not present in standard header)
    // -------------------------------------------------------------------------

    private DataBlock.BBlock parseBBlock(byte type, byte version, long tsUs,
                                         ByteBuffer p, int payloadLen) throws IOException {
        requirePayloadLen(type, payloadLen, 8);

        float pressurePa = p.getFloat();
        float tempC      = p.getFloat();

        // The standard vuams_block_header_t carries no seq field; use 0 as sentinel.
        return new DataBlock.BBlock(type, version, 0, tsUs, pressurePa, tempC);
    }

    // -------------------------------------------------------------------------
    // V-block — 604 bytes payload:
    //   uint16 seq (2), int16[100] ax (200), int16[100] ay (200), int16[100] az (200), uint16 crc (2)
    // -------------------------------------------------------------------------

    private DataBlock.VBlock parseVBlock(byte type, byte version, long tsUs,
                                         ByteBuffer p, int payloadLen) throws IOException {
        // payload = 2 (seq) + 3*200 (axes) + 2 (crc) = 604 bytes
        requirePayloadLen(type, payloadLen, 604);

        int seq        = p.getShort() & 0xFFFF;   // uint16 sequence number
        short[] ax     = readShorts(p, 100);
        short[] ay     = readShorts(p, 100);
        short[] az     = readShorts(p, 100);
        /* crc16 — read and discard (reserved for future validation) */
        p.getShort();

        return new DataBlock.VBlock(type, version, seq, tsUs, ax, ay, az);
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    private static short[] readShorts(ByteBuffer buf, int count) {
        short[] arr = new short[count];
        for (int i = 0; i < count; i++) arr[i] = buf.getShort();
        return arr;
    }

    private static void requirePayloadLen(byte type, int actual, int expected) throws IOException {
        if (actual != expected) {
            throw new IOException(
                    "Block '%c': expected payload_len=%d, got %d"
                            .formatted((char) (type & 0xFF), expected, actual));
        }
    }

    // -------------------------------------------------------------------------
    // Subject metadata header parsing
    // -------------------------------------------------------------------------

    private static boolean isMagic(byte[] probe) {
        return probe[0] == METADATA_MAGIC[0]
            && probe[1] == METADATA_MAGIC[1]
            && probe[2] == METADATA_MAGIC[2]
            && probe[3] == METADATA_MAGIC[3];
    }

    /**
     * Parses the subject metadata header. Called after the 4-byte magic has already
     * been consumed. The next 2 bytes are {@code header_len} (uint16LE, total section
     * length including the 4 magic bytes and the 2 header_len bytes themselves).
     */
    private static SubjectMetadata parseSubjectMetadata(DataInputStream in) throws IOException {
        // header_len covers: 4 (magic) + 2 (header_len) + content
        byte[] lenBytes = new byte[2];
        in.readFully(lenBytes);
        int headerLen = ByteBuffer.wrap(lenBytes).order(ByteOrder.LITTLE_ENDIAN).getShort() & 0xFFFF;
        int contentLen = headerLen - 6; // subtract magic(4) + header_len field(2)
        if (contentLen < 6) {
            throw new IOException("Subject metadata header too short: header_len=" + headerLen);
        }

        byte[] content = new byte[contentLen];
        in.readFully(content);
        ByteBuffer c = ByteBuffer.wrap(content).order(ByteOrder.LITTLE_ENDIAN);

        float electrodeDistanceCm = c.getFloat();                           // 4 bytes

        int subjectIdLen = c.getShort() & 0xFFFF;                           // 2 bytes
        byte[] subjectIdBytes = new byte[subjectIdLen];
        c.get(subjectIdBytes);
        String subjectId = new String(subjectIdBytes, StandardCharsets.UTF_8);

        int recordingDateLen = c.getShort() & 0xFFFF;                       // 2 bytes
        byte[] recordingDateBytes = new byte[recordingDateLen];
        c.get(recordingDateBytes);
        String recordingDate = new String(recordingDateBytes, StandardCharsets.UTF_8);

        return new SubjectMetadata(electrodeDistanceCm, subjectId, recordingDate);
    }
}
