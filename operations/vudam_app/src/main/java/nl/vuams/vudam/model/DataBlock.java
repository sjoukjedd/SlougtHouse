package nl.vuams.vudam.model;

/**
 * Sealed interface hierarchy mirroring the packed C structs in data_blocks.h.
 *
 * <p>Block wire format:
 * <pre>
 *   Offset  Size  Field
 *   0       1     type      (BLOCK_TYPE_* — 'A','I','M','P','S','T')
 *   1       1     version   (BLOCK_VERSION = 0x01)
 *   2       2     payload_len  (uint16 LE — bytes after the 16-byte header)
 *   4       8     timestamp_us (uint64 LE — µs from esp_timer_get_time())
 *   12      4     (padding to 16 bytes — implicit in packed struct alignment)
 *   16      N     payload   (block-specific, see subtype javadoc)
 * </pre>
 *
 * <p>Note: the C header uses __attribute__((packed)) so there is no padding
 * between fields. The 16-byte total header size is:
 * 1 (type) + 1 (version) + 2 (payload_len) + 8 (timestamp_us) + 4 (reserved/pad) = 16.
 * VUAFileReader accounts for this when seeking past the header.
 */
public sealed interface DataBlock
        permits DataBlock.ABlock,
                DataBlock.BBlock,
                DataBlock.IBlock,
                DataBlock.MBlock,
                DataBlock.PBlock,
                DataBlock.SBlock,
                DataBlock.TBlock,
                DataBlock.VBlock {

    /** Block type byte as defined by BLOCK_TYPE_* constants. */
    byte type();

    /** Schema version — always 0x01 for files written by current firmware. */
    byte version();

    /** Timestamp of the first sample in this block, microseconds (esp_timer). */
    long timestampUs();

    // -------------------------------------------------------------------------
    // A-block — ECG raw, 1 kHz, 2 channels, 250 samples per block
    //   payload_len = 2 × 250 × 4 = 2000 bytes
    //   int32_t ecg1[250]  — channel 1, 24-bit value sign-extended to 32 bits
    //   int32_t ecg2[250]  — channel 2
    // -------------------------------------------------------------------------

    /**
     * ECG raw-sample block.
     *
     * @param type        block type byte — always {@code 0x41} ('A')
     * @param version     schema version
     * @param timestampUs timestamp of sample[0], µs
     * @param ecg1        250 samples, channel 1 (ADS1256 24-bit, sign-extended)
     * @param ecg2        250 samples, channel 2
     */
    record ABlock(
            byte type,
            byte version,
            long timestampUs,
            int[] ecg1,
            int[] ecg2
    ) implements DataBlock {
        public static final byte TYPE = 0x41;

        public ABlock {
            if (ecg1.length != 250) throw new IllegalArgumentException("ecg1 must be 250 samples");
            if (ecg2.length != 250) throw new IllegalArgumentException("ecg2 must be 250 samples");
        }
    }

    // -------------------------------------------------------------------------
    // I-block — ICG-derived haemodynamic parameters, one entry per heartbeat
    //   payload_len = 6 × 4 = 24 bytes
    // -------------------------------------------------------------------------

    /**
     * ICG-derived haemodynamic parameter block (one per heartbeat).
     *
     * @param type        block type byte — always {@code 0x49} ('I')
     * @param version     schema version
     * @param timestampUs timestamp of the beat, µs
     * @param z0          baseline impedance [Ohm]
     * @param dZdtPeak    peak –dZ/dt amplitude [Ohm/s]
     * @param pepMs       pre-ejection period [ms]
     * @param lvetMs      left ventricular ejection time [ms]
     * @param coLpm       cardiac output estimate [L/min]
     * @param svMl        stroke volume estimate [mL]
     */
    record IBlock(
            byte type,
            byte version,
            long timestampUs,
            float z0,
            float dZdtPeak,
            float pepMs,
            float lvetMs,
            float coLpm,
            float svMl
    ) implements DataBlock {
        public static final byte TYPE = 0x49;
    }

    // -------------------------------------------------------------------------
    // M-block — IMU raw, 100 Hz, 10 samples per block (100 ms window)
    //   payload_len = 9 channels × 10 samples × 2 bytes = 180 bytes
    // -------------------------------------------------------------------------

    /**
     * IMU raw-sample block (ICM-20948).
     *
     * <p>All arrays are 10 elements, 100 Hz, so each block covers 100 ms.
     *
     * @param type        block type byte — always {@code 0x4D} ('M')
     * @param version     schema version
     * @param timestampUs timestamp of sample[0], µs
     * @param ax          accelerometer X [raw LSB]
     * @param ay          accelerometer Y
     * @param az          accelerometer Z
     * @param gx          gyroscope X [raw LSB]
     * @param gy          gyroscope Y
     * @param gz          gyroscope Z
     * @param mx          magnetometer X [raw LSB]
     * @param my          magnetometer Y
     * @param mz          magnetometer Z
     */
    record MBlock(
            byte type,
            byte version,
            long timestampUs,
            short[] ax, short[] ay, short[] az,
            short[] gx, short[] gy, short[] gz,
            short[] mx, short[] my, short[] mz
    ) implements DataBlock {
        public static final byte TYPE = 0x4D;

        public MBlock {
            validateLen(ax, "ax"); validateLen(ay, "ay"); validateLen(az, "az");
            validateLen(gx, "gx"); validateLen(gy, "gy"); validateLen(gz, "gz");
            validateLen(mx, "mx"); validateLen(my, "my"); validateLen(mz, "mz");
        }

        private static void validateLen(short[] arr, String name) {
            if (arr.length != 10)
                throw new IllegalArgumentException("IMU channel " + name + " must be 10 samples");
        }
    }

    // -------------------------------------------------------------------------
    // P-block — PPG (MAX30101), one block per poll cycle (~10 ms)
    //   payload_len = 4+4+4+1+1 = 14 bytes
    // -------------------------------------------------------------------------

    /**
     * PPG block (MAX30101).
     *
     * @param type         block type byte — always {@code 0x50} ('P')
     * @param version      schema version
     * @param timestampUs  timestamp, µs
     * @param ppgRed       red LED raw count
     * @param ppgIr        IR LED raw count
     * @param spo2Pct      SpO2 estimate [%], NaN if invalid
     * @param hrPpg        heart rate from PPG [bpm], 0 = invalid
     * @param ppgValid     true if finger present and signal good
     */
    record PBlock(
            byte type,
            byte version,
            long timestampUs,
            int ppgRed,
            int ppgIr,
            float spo2Pct,
            int hrPpg,
            boolean ppgValid
    ) implements DataBlock {
        public static final byte TYPE = 0x50;
    }

    // -------------------------------------------------------------------------
    // S-block — Skin Conductance Level / EDA (AD5933)
    //   payload_len = 4+4+1 = 9 bytes
    // -------------------------------------------------------------------------

    /**
     * SCL/EDA block.
     *
     * @param type              block type byte — always {@code 0x53} ('S')
     * @param version           schema version
     * @param timestampUs       timestamp, µs
     * @param sclTonicUs        tonic SCL [µS]
     * @param sclPhasicUs       phasic component [µS]
     * @param electrodeContact  true = good skin contact
     */
    record SBlock(
            byte type,
            byte version,
            long timestampUs,
            float sclTonicUs,
            float sclPhasicUs,
            boolean electrodeContact
    ) implements DataBlock {
        public static final byte TYPE = 0x53;
    }

    // -------------------------------------------------------------------------
    // T-block — Temperature (TMP117)
    //   payload_len = 4+2 = 6 bytes
    // -------------------------------------------------------------------------

    /**
     * Temperature block (TMP117).
     *
     * @param type         block type byte — always {@code 0x54} ('T')
     * @param version      schema version
     * @param timestampUs  timestamp, µs
     * @param skinTempC    skin temperature [°C]
     * @param tempRaw      TMP117 raw register value
     */
    record TBlock(
            byte type,
            byte version,
            long timestampUs,
            float skinTempC,
            short tempRaw
    ) implements DataBlock {
        public static final byte TYPE = 0x54;
    }

    // -------------------------------------------------------------------------
    // V-block — High-ODR accelerometer for Speech Activity Detection (SAD)
    //   1 kHz, 100 samples per block (100 ms window), SD-only
    //   payload_len = 3 × 100 × 2 = 600 bytes
    //   int16_t ax[100], ay[100], az[100]
    // -------------------------------------------------------------------------

    /**
     * High-ODR accelerometer block for Speech Activity Detection (SP-002).
     *
     * <p>Carries 100 samples at 1 kHz on three accelerometer axes, covering 100 ms.
     * The gyroscope and magnetometer are NOT included — they remain in the 100 Hz M-block.
     *
     * @param type        block type byte — always {@code 0x56} ('V')
     * @param version     schema version
     * @param seq         rolling sequence number
     * @param timestampUs timestamp of sample[0], µs
     * @param ax          100 accelerometer-X samples [raw LSB, 1 kHz]
     * @param ay          100 accelerometer-Y samples
     * @param az          100 accelerometer-Z samples
     */
    record VBlock(
            byte type,
            byte version,
            int seq,
            long timestampUs,
            short[] ax,
            short[] ay,
            short[] az
    ) implements DataBlock {
        public static final byte TYPE = 0x56;

        public VBlock {
            validateLen(ax, "ax");
            validateLen(ay, "ay");
            validateLen(az, "az");
        }

        private static void validateLen(short[] arr, String name) {
            if (arr.length != 100)
                throw new IllegalArgumentException("V-block channel " + name + " must be 100 samples");
        }
    }

    // -------------------------------------------------------------------------
    // B-block — Barometric pressure / altitude (BMP390), 25 Hz
    //   payload_len = 4 + 4 = 8 bytes
    //   float baro_pressure_pa, float baro_temp_c
    // -------------------------------------------------------------------------

    /**
     * Barometric pressure block (BMP390) for HAR stair detection (SP-002).
     *
     * <p>One sample per block at up to 25 Hz. Altitude is derived from pressure
     * in the analyser using the hypsometric approximation (ΔP / 12.0 Pa/m).
     *
     * @param type           block type byte — always {@code 0x42} ('B')
     * @param version        schema version
     * @param seq            rolling sequence number
     * @param timestampUs    timestamp of the pressure sample, µs
     * @param baroPressurePa compensated pressure [Pa]
     * @param baroTempC      compensated temperature [°C]
     */
    record BBlock(
            byte type,
            byte version,
            int seq,
            long timestampUs,
            float baroPressurePa,
            float baroTempC
    ) implements DataBlock {
        public static final byte TYPE = 0x42;
    }
}
