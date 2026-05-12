package nl.vuams.vudam.ui;

import javafx.application.Platform;
import javafx.beans.property.SimpleDoubleProperty;
import javafx.beans.property.SimpleFloatProperty;
import javafx.beans.property.SimpleIntegerProperty;
import javafx.beans.property.SimpleStringProperty;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.fxml.FXML;
import javafx.scene.chart.LineChart;
import javafx.scene.chart.XYChart;
import javafx.scene.control.Alert;
import javafx.scene.control.Label;
import javafx.scene.control.MenuItem;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.stage.FileChooser;
import javafx.stage.Stage;
import nl.vuams.vudam.analysis.CSIAnalyser;
import nl.vuams.vudam.analysis.HRVAnalyser;
import nl.vuams.vudam.analysis.HarAnalyser;
import nl.vuams.vudam.analysis.RPeakDetector;
import nl.vuams.vudam.analysis.SadAnalyser;
import nl.vuams.vudam.io.VUAFileReader;
import nl.vuams.vudam.model.DataBlock;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

/**
 * JavaFX controller for main.fxml.
 *
 * <p>Responsibilities:
 * <ul>
 *   <li>File open dialog → parse .vua file via {@link VUAFileReader}</li>
 *   <li>Populate ECG, ICG and IMU waveform charts from parsed blocks</li>
 *   <li>Delegate R-peak detection to {@link RPeakDetector} and HRV computation
 *       to {@link HRVAnalyser}</li>
 *   <li>Populate HRV and ICG metrics tables</li>
 *   <li>CSV export of all computed metrics</li>
 * </ul>
 */
public class MainController {

    // ---- Charts ----
    @FXML private LineChart<Number, Number> ecgChart;
    @FXML private LineChart<Number, Number> icgChart;
    @FXML private LineChart<Number, Number> imuAccChart;
    @FXML private LineChart<Number, Number> rriChart;

    // ---- HRV table ----
    @FXML private TableView<HrvRow>           hrvTable;
    @FXML private TableColumn<HrvRow, String> hrvMetricCol;
    @FXML private TableColumn<HrvRow, Number> hrvValueCol;
    @FXML private TableColumn<HrvRow, String> hrvUnitCol;

    // ---- ICG table ----
    @FXML private TableView<IcgRow>           icgTable;
    @FXML private TableColumn<IcgRow, Number> icgBeatCol;
    @FXML private TableColumn<IcgRow, Number> icgPepCol;
    @FXML private TableColumn<IcgRow, Number> icgLvetCol;
    @FXML private TableColumn<IcgRow, Number> icgSvCol;
    @FXML private TableColumn<IcgRow, Number> icgCoCol;
    @FXML private TableColumn<IcgRow, Number> icgZ0Col;

    // ---- CSI table ----
    @FXML private TableView<CsiRow>           csiTable;
    @FXML private TableColumn<CsiRow, Number> csiEpochCol;
    @FXML private TableColumn<CsiRow, Number> csiScoreCol;
    @FXML private TableColumn<CsiRow, Number> csiPepZCol;
    @FXML private TableColumn<CsiRow, Number> csiSclZCol;
    @FXML private TableColumn<CsiRow, Number> csiRmssdZCol;
    @FXML private TableColumn<CsiRow, Number> csiScrZCol;

    // ---- Menu items that need enabling after load ----
    @FXML private MenuItem menuExportCsv;
    @FXML private MenuItem menuDetectRpeaks;
    @FXML private MenuItem menuComputeHrv;
    @FXML private MenuItem menuDetectBpoints;
    @FXML private MenuItem menuComputeCsi;

    // ---- Status bar ----
    @FXML private Label statusLabel;

    // ---- Activiteit tab ----
    @FXML private TabPane mainTabPane;
    @FXML private TableView<ActivityRow>                activityTable;
    @FXML private TableColumn<ActivityRow, String>      actTimeCol;
    @FXML private TableColumn<ActivityRow, String>      actClassCol;
    @FXML private TableColumn<ActivityRow, Number>      actCadenceCol;
    @FXML private TableColumn<ActivityRow, Number>      actIntensityCol;
    @FXML private TableColumn<ActivityRow, Number>      actSpeakingCol;
    @FXML private TableColumn<ActivityRow, Number>      actAltCol;

    // ---- State ----
    private Stage primaryStage;
    private List<DataBlock> loadedBlocks = List.of();
    private List<Double>    rriMs        = List.of();  // R-R intervals in ms
    private File            currentFile;
    private CSIAnalyser     csiAnalyser  = new CSIAnalyser();

    /** Called by {@link nl.vuams.vudam.VUDAMSApp} after FXML load. */
    public void setPrimaryStage(Stage stage) {
        this.primaryStage = stage;
    }

    @FXML
    private void initialize() {
        // HRV table cell-value factories
        hrvMetricCol.setCellValueFactory(cd -> new SimpleStringProperty(cd.getValue().metric()));
        hrvValueCol .setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().value()));
        hrvUnitCol  .setCellValueFactory(cd -> new SimpleStringProperty(cd.getValue().unit()));

        // ICG table cell-value factories
        icgBeatCol.setCellValueFactory(cd -> new SimpleIntegerProperty(cd.getValue().beat()));
        icgPepCol .setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().pepMs()));
        icgLvetCol.setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().lvetMs()));
        icgSvCol  .setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().svMl()));
        icgCoCol  .setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().coLpm()));
        icgZ0Col  .setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().z0()));

        // CSI table cell-value factories (FXML-declared tab)
        if (csiTable != null) {
            csiEpochCol .setCellValueFactory(cd -> new SimpleIntegerProperty(cd.getValue().epoch()));
            csiScoreCol .setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().score()));
            csiPepZCol  .setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().pepZ()));
            csiSclZCol  .setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().sclZ()));
            csiRmssdZCol.setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().rmssdZ()));
            csiScrZCol  .setCellValueFactory(cd -> new SimpleDoubleProperty(cd.getValue().scrZ()));
        }

        // Activiteit tab — build programmatically if FXML does not declare it
        initActivityTab();
    }

    /**
     * Builds the "Activiteit" tab and its TableView when the FXML does not
     * already contain it.  If the tab pane and columns were injected via FXML,
     * this method configures the cell-value factories on them; otherwise it
     * creates the entire tab structure and appends it to {@code mainTabPane}.
     */
    private void initActivityTab() {
        if (activityTable == null) {
            // Create the table and its columns programmatically
            activityTable = new TableView<>();
            activityTable.setColumnResizePolicy(TableView.CONSTRAINED_RESIZE_POLICY);

            actTimeCol      = new TableColumn<>("Tijdstip");
            actClassCol     = new TableColumn<>("Activiteit");
            actCadenceCol   = new TableColumn<>("Cadans (spm)");
            actIntensityCol = new TableColumn<>("Intensiteit");
            actSpeakingCol  = new TableColumn<>("Spraak %");
            actAltCol       = new TableColumn<>("Hoogte Δ (m)");

            activityTable.getColumns().addAll(
                    actTimeCol, actClassCol, actCadenceCol,
                    actIntensityCol, actSpeakingCol, actAltCol);

            if (mainTabPane != null) {
                Tab tab = new Tab("Activiteit", activityTable);
                tab.setClosable(false);
                mainTabPane.getTabs().add(tab);
            }
        }

        // Wire cell-value factories
        actTimeCol.setCellValueFactory(
                cd -> new SimpleStringProperty(cd.getValue().timeLabel()));
        actClassCol.setCellValueFactory(
                cd -> new SimpleStringProperty(cd.getValue().activityClass()));
        actCadenceCol.setCellValueFactory(
                cd -> new SimpleFloatProperty(cd.getValue().cadenceSpm()));
        actIntensityCol.setCellValueFactory(
                cd -> new SimpleFloatProperty(cd.getValue().motionIntensity()));
        actSpeakingCol.setCellValueFactory(
                cd -> new SimpleFloatProperty(cd.getValue().speakingPct()));
        actAltCol.setCellValueFactory(
                cd -> new SimpleFloatProperty(cd.getValue().altitudeChangeM()));
    }

    // =========================================================================
    // Menu handlers
    // =========================================================================

    @FXML
    private void handleOpen() {
        FileChooser chooser = new FileChooser();
        chooser.setTitle("Open VU-AMS recording");
        chooser.getExtensionFilters().add(
                new FileChooser.ExtensionFilter("VU-AMS data files (*.vua)", "*.vua"));

        File file = chooser.showOpenDialog(primaryStage);
        if (file == null) return;

        currentFile = file;
        loadFile(file.toPath());
    }

    @FXML
    private void handleExportCsv() {
        if (loadedBlocks.isEmpty()) return;

        FileChooser chooser = new FileChooser();
        chooser.setTitle("Export metrics to CSV");
        chooser.setInitialFileName(
                currentFile != null ? baseName(currentFile.getName()) + "_metrics.csv" : "metrics.csv");
        chooser.getExtensionFilters().add(
                new FileChooser.ExtensionFilter("CSV files (*.csv)", "*.csv"));

        File dest = chooser.showSaveDialog(primaryStage);
        if (dest == null) return;

        exportCsv(dest);
    }

    @FXML
    private void handleDetectRpeaks() {
        if (loadedBlocks.isEmpty()) return;

        setStatus("Detecting R-peaks…");
        new Thread(() -> {
            List<DataBlock.ABlock> aBlocks = loadedBlocks.stream()
                    .filter(b -> b instanceof DataBlock.ABlock)
                    .map(b -> (DataBlock.ABlock) b)
                    .toList();

            double[] ecgSignal = assembleEcgCh1(aBlocks);
            List<Integer> peaks = RPeakDetector.detect(ecgSignal, 1000.0);

            rriMs = computeRri(peaks, 1000.0);

            Platform.runLater(() -> {
                populateRriChart(rriMs);
                setStatus("R-peaks detected: %d peaks, %d RR intervals."
                        .formatted(peaks.size(), rriMs.size()));
                menuComputeHrv.setDisable(false);
            });
        }, "rpeaks-thread").start();
    }

    @FXML
    private void handleComputeHrv() {
        if (rriMs.isEmpty()) return;

        setStatus("Computing HRV metrics…");
        new Thread(() -> {
            HRVAnalyser.Metrics m = HRVAnalyser.compute(rriMs);
            ObservableList<HrvRow> rows = FXCollections.observableArrayList(
                    new HrvRow("Mean RRI",  m.meanRri(),  "ms"),
                    new HrvRow("SDNN",      m.sdnn(),     "ms"),
                    new HrvRow("RMSSD",     m.rmssd(),    "ms"),
                    new HrvRow("pNN50",     m.pnn50(),    "%"),
                    new HrvRow("LF power",  m.lfPower(),  "ms²"),
                    new HrvRow("HF power",  m.hfPower(),  "ms²"),
                    new HrvRow("LF/HF",     m.lfHfRatio(),"")
            );
            Platform.runLater(() -> {
                hrvTable.setItems(rows);
                setStatus("HRV metrics computed.");
            });
        }, "hrv-thread").start();
    }

    @FXML
    private void handleComputeCsi() {
        if (loadedBlocks.isEmpty()) return;

        setStatus("Computing CSI epochs…");
        new Thread(() -> {
            // Collect IBlocks and SBlocks sorted by timestamp
            List<DataBlock.IBlock> iBlocks = loadedBlocks.stream()
                    .filter(b -> b instanceof DataBlock.IBlock)
                    .map(b -> (DataBlock.IBlock) b)
                    .sorted(java.util.Comparator.comparingLong(DataBlock.IBlock::timestampUs))
                    .toList();
            List<DataBlock.SBlock> sBlocks = loadedBlocks.stream()
                    .filter(b -> b instanceof DataBlock.SBlock)
                    .map(b -> (DataBlock.SBlock) b)
                    .sorted(java.util.Comparator.comparingLong(DataBlock.SBlock::timestampUs))
                    .toList();

            if (iBlocks.isEmpty()) {
                Platform.runLater(() -> {
                    setStatus("CSI: no IBlock data available.");
                    showInfo("No ICG data", "This recording contains no I-blocks (ICG parameters). Cannot compute CSI.");
                });
                return;
            }

            final long EPOCH_US = 30_000_000L; // 30 s in µs
            long firstTs = iBlocks.get(0).timestampUs();

            // Build a list of IBlock timestamps for RRI computation from IBlocks
            // (beat-to-beat intervals from ICG timestamps serve as RRI proxy when
            //  dedicated R-peak detection has not been run yet)
            List<Long> beatTimesUs = iBlocks.stream()
                    .map(DataBlock.IBlock::timestampUs)
                    .toList();

            csiAnalyser.reset();
            ObservableList<CsiRow> rows = FXCollections.observableArrayList();
            int epochIndex = 0;

            long epochStart = firstTs;
            while (true) {
                long epochEnd = epochStart + EPOCH_US;

                // Gather IBlocks in this epoch
                final long es = epochStart;
                final long ee = epochEnd;
                List<DataBlock.IBlock> epochIBlocks = iBlocks.stream()
                        .filter(b -> b.timestampUs() >= es && b.timestampUs() < ee)
                        .toList();
                if (epochIBlocks.isEmpty()) break;

                // PEP: mean over epoch
                double pepMs = epochIBlocks.stream()
                        .mapToDouble(DataBlock.IBlock::pepMs)
                        .average()
                        .orElse(Double.NaN);

                // RMSSD from beat-to-beat intervals within this epoch
                List<Long> epochBeatTimes = beatTimesUs.stream()
                        .filter(t -> t >= es && t < ee)
                        .toList();
                double rmssdMs;
                if (epochBeatTimes.size() >= 2) {
                    double sumSq = 0.0;
                    int count = 0;
                    for (int i = 1; i < epochBeatTimes.size(); i++) {
                        double diffMs = (epochBeatTimes.get(i) - epochBeatTimes.get(i - 1)) / 1000.0;
                        sumSq += diffMs * diffMs;
                        count++;
                    }
                    rmssdMs = Math.sqrt(sumSq / count);
                } else {
                    // Fallback: use global rriMs if R-peak detection was run
                    rmssdMs = rriMs.isEmpty() ? Double.NaN : computeRmssd(rriMs);
                }

                // SCL: mean tonic from SBlocks in this epoch
                List<DataBlock.SBlock> epochSBlocks = sBlocks.stream()
                        .filter(b -> b.timestampUs() >= es && b.timestampUs() < ee)
                        .toList();
                double sclUs = epochSBlocks.stream()
                        .mapToDouble(DataBlock.SBlock::sclTonicUs)
                        .average()
                        .orElse(0.0);

                // SCR rate: fallback to 0.0 (phasic detection not yet implemented)
                double scrRate = 0.0;

                if (!Double.isFinite(pepMs) || pepMs <= 0.0
                        || !Double.isFinite(rmssdMs) || rmssdMs <= 0.0) {
                    epochStart = epochEnd;
                    epochIndex++;
                    continue;
                }

                CSIAnalyser.EpochInput input = new CSIAnalyser.EpochInput(
                        pepMs, sclUs, rmssdMs, scrRate);
                CSIAnalyser.CSIResult result = csiAnalyser.addEpoch(input);

                if (result != null) {
                    final int ep = epochIndex;
                    rows.add(new CsiRow(ep, result.score, result.pepZ,
                            result.sclZ, result.rmssdZ, result.scrZ));
                }

                epochStart = epochEnd;
                epochIndex++;
            }

            Platform.runLater(() -> {
                if (csiTable != null) {
                    csiTable.setItems(rows);
                }
                setStatus("CSI computed: %d epochs scored (first 4 used as baseline)."
                        .formatted(rows.size()));
            });
        }, "csi-thread").start();
    }

    @FXML
    private void handleDetectBpoints() {
        if (loadedBlocks.isEmpty()) return;

        // I-blocks already carry pre-computed ICG parameters from firmware.
        // Populate the ICG table directly from IBlock data.
        List<DataBlock.IBlock> iBlocks = loadedBlocks.stream()
                .filter(b -> b instanceof DataBlock.IBlock)
                .map(b -> (DataBlock.IBlock) b)
                .toList();

        if (iBlocks.isEmpty()) {
            showInfo("No ICG data", "This recording contains no I-blocks (ICG parameters).");
            return;
        }

        ObservableList<IcgRow> rows = FXCollections.observableArrayList();
        for (int i = 0; i < iBlocks.size(); i++) {
            DataBlock.IBlock ib = iBlocks.get(i);
            rows.add(new IcgRow(i + 1, ib.pepMs(), ib.lvetMs(), ib.svMl(), ib.coLpm(), ib.z0()));
        }
        icgTable.setItems(rows);
        setStatus("ICG data loaded: %d beats.".formatted(iBlocks.size()));
    }

    @FXML private void handleZoomIn()    { adjustXRange(0.5); }
    @FXML private void handleZoomOut()   { adjustXRange(2.0); }
    @FXML private void handleZoomReset() { adjustXRange(-1);  }

    @FXML
    private void handleAbout() {
        Alert a = new Alert(Alert.AlertType.INFORMATION);
        a.setTitle("About VU-DAMS");
        a.setHeaderText("VU-DAMS — VU-AMS Desktop Analysis & Management System");
        a.setContentText("Reads .vua binary recordings from the VU-AMS wearable device.\n" +
                "Analyses ECG, ICG, IMU signals offline.\n\n" +
                "Slow Horses / VU Amsterdam — 2026");
        a.showAndWait();
    }

    @FXML
    private void handleExit() {
        Platform.exit();
    }

    // =========================================================================
    // File loading
    // =========================================================================

    private void loadFile(Path path) {
        setStatus("Loading " + path.getFileName() + " …");
        new Thread(() -> {
            try {
                VUAFileReader reader = new VUAFileReader();
                List<DataBlock> blocks = reader.read(path);

                // Separate block types needed for HAR and SAD
                List<DataBlock.MBlock> mBlocks = blocks.stream()
                        .filter(b -> b instanceof DataBlock.MBlock)
                        .map(b -> (DataBlock.MBlock) b)
                        .toList();
                List<DataBlock.BBlock> bBlocks = blocks.stream()
                        .filter(b -> b instanceof DataBlock.BBlock)
                        .map(b -> (DataBlock.BBlock) b)
                        .toList();
                List<DataBlock.VBlock> vBlocks = blocks.stream()
                        .filter(b -> b instanceof DataBlock.VBlock)
                        .map(b -> (DataBlock.VBlock) b)
                        .toList();

                // Run HAR and SAD on background thread (potentially slow for long recordings)
                List<HarAnalyser.HarResult> harResults = HarAnalyser.analyse(mBlocks, bBlocks);
                List<SadAnalyser.SadResult> sadResults = SadAnalyser.analyse(vBlocks);

                Platform.runLater(() -> {
                    loadedBlocks = blocks;
                    rriMs = List.of();
                    csiAnalyser.reset();
                    populateWaveforms(blocks);
                    populateActivityTable(harResults, sadResults);
                    enablePostLoadMenus();
                    setStatus("Loaded %s — %d blocks, %d HAR epochs, %d SAD epochs."
                            .formatted(path.getFileName(), blocks.size(),
                                       harResults.size(), sadResults.size()));
                });
            } catch (IOException ex) {
                Platform.runLater(() -> {
                    setStatus("Error: " + ex.getMessage());
                    showError("Failed to load file", ex.getMessage());
                });
            }
        }, "file-loader").start();
    }

    // =========================================================================
    // Chart population
    // =========================================================================

    private void populateWaveforms(List<DataBlock> blocks) {
        populateEcgChart(blocks);
        populateIcgChart(blocks);
        populateImuChart(blocks);
    }

    private void populateEcgChart(List<DataBlock> blocks) {
        XYChart.Series<Number, Number> ch1 = new XYChart.Series<>();
        ch1.setName("ECG ch1");
        XYChart.Series<Number, Number> ch2 = new XYChart.Series<>();
        ch2.setName("ECG ch2");

        long firstTs = -1;
        for (DataBlock block : blocks) {
            if (!(block instanceof DataBlock.ABlock ab)) continue;
            if (firstTs < 0) firstTs = ab.timestampUs();

            double baseTimeSec = (ab.timestampUs() - firstTs) / 1_000_000.0;
            double stepSec = 1.0 / 1000.0; // 1 kHz

            for (int i = 0; i < 250; i++) {
                double t = baseTimeSec + i * stepSec;
                // Downsample to every 4th point for chart performance (250 Hz display)
                if (i % 4 == 0) {
                    ch1.getData().add(new XYChart.Data<>(t, ab.ecg1()[i]));
                    ch2.getData().add(new XYChart.Data<>(t, ab.ecg2()[i]));
                }
            }
        }

        ecgChart.getData().clear();
        ecgChart.getData().addAll(ch1, ch2);
    }

    private void populateIcgChart(List<DataBlock> blocks) {
        XYChart.Series<Number, Number> svSeries  = new XYChart.Series<>();
        svSeries.setName("SV (mL)");
        XYChart.Series<Number, Number> pepSeries = new XYChart.Series<>();
        pepSeries.setName("PEP (ms)");

        long firstTs = -1;
        for (DataBlock block : blocks) {
            if (!(block instanceof DataBlock.IBlock ib)) continue;
            if (firstTs < 0) firstTs = ib.timestampUs();

            double t = (ib.timestampUs() - firstTs) / 1_000_000.0;
            svSeries.getData().add(new XYChart.Data<>(t, ib.svMl()));
            pepSeries.getData().add(new XYChart.Data<>(t, ib.pepMs()));
        }

        icgChart.getData().clear();
        icgChart.getData().addAll(svSeries, pepSeries);
    }

    private void populateImuChart(List<DataBlock> blocks) {
        XYChart.Series<Number, Number> axSeries = new XYChart.Series<>(); axSeries.setName("ax");
        XYChart.Series<Number, Number> aySeries = new XYChart.Series<>(); aySeries.setName("ay");
        XYChart.Series<Number, Number> azSeries = new XYChart.Series<>(); azSeries.setName("az");

        long firstTs = -1;
        double stepSec = 1.0 / 100.0; // 100 Hz

        for (DataBlock block : blocks) {
            if (!(block instanceof DataBlock.MBlock mb)) continue;
            if (firstTs < 0) firstTs = mb.timestampUs();

            double baseTimeSec = (mb.timestampUs() - firstTs) / 1_000_000.0;
            for (int i = 0; i < 10; i++) {
                double t = baseTimeSec + i * stepSec;
                axSeries.getData().add(new XYChart.Data<>(t, mb.ax()[i]));
                aySeries.getData().add(new XYChart.Data<>(t, mb.ay()[i]));
                azSeries.getData().add(new XYChart.Data<>(t, mb.az()[i]));
            }
        }

        imuAccChart.getData().clear();
        imuAccChart.getData().addAll(axSeries, aySeries, azSeries);
    }

    /**
     * Merges HAR and SAD epoch results into the Activiteit TableView.
     *
     * <p>HAR and SAD epochs are aligned by start time.  SAD data is matched
     * to the nearest HAR epoch (same start millisecond when available).
     * Epochs present only in HAR are shown with NaN speaking fraction.
     */
    private void populateActivityTable(List<HarAnalyser.HarResult> harResults,
                                       List<SadAnalyser.SadResult> sadResults) {
        if (activityTable == null) return;

        ObservableList<ActivityRow> rows = FXCollections.observableArrayList();

        // Build a quick lookup: epochStartMs → SadResult
        java.util.Map<Long, SadAnalyser.SadResult> sadMap = new java.util.HashMap<>();
        for (SadAnalyser.SadResult sr : sadResults) {
            if (sr.sadAvailable()) {
                sadMap.put(sr.epochStartMs(), sr);
            }
        }

        for (HarAnalyser.HarResult hr : harResults) {
            SadAnalyser.SadResult sr = sadMap.get(hr.windowStartMs());

            long startMs = hr.windowStartMs();
            long mm  = (startMs / 60_000L) % 60;
            long ss  = (startMs / 1_000L)  % 60;
            String timeLabel = "%02d:%02d".formatted(mm, ss);

            float speakingPct = (sr != null && sr.sadAvailable())
                    ? sr.speakingFraction() * 100.0f
                    : Float.NaN;

            rows.add(new ActivityRow(
                    timeLabel,
                    hr.activityClass().name(),
                    hr.cadenceSpm(),
                    hr.motionIntensity(),
                    speakingPct,
                    hr.altitudeChangeM()
            ));
        }

        activityTable.setItems(rows);
    }

    private void populateRriChart(List<Double> rri) {
        XYChart.Series<Number, Number> series = new XYChart.Series<>();
        series.setName("RRI");
        for (int i = 0; i < rri.size(); i++) {
            series.getData().add(new XYChart.Data<>(i + 1, rri.get(i)));
        }
        rriChart.getData().clear();
        rriChart.getData().add(series);
    }

    // =========================================================================
    // Analysis helpers
    // =========================================================================

    private static double[] assembleEcgCh1(List<DataBlock.ABlock> aBlocks) {
        double[] signal = new double[aBlocks.size() * 250];
        int idx = 0;
        for (DataBlock.ABlock ab : aBlocks) {
            for (int i = 0; i < 250; i++) {
                signal[idx++] = ab.ecg1()[i];
            }
        }
        return signal;
    }

    private static double computeRmssd(List<Double> rri) {
        if (rri.size() < 2) return Double.NaN;
        double sumSq = 0.0;
        for (int i = 1; i < rri.size(); i++) {
            double diff = rri.get(i) - rri.get(i - 1);
            sumSq += diff * diff;
        }
        return Math.sqrt(sumSq / (rri.size() - 1));
    }

    private static List<Double> computeRri(List<Integer> peakIndices, double sampleRateHz) {
        List<Double> rri = new ArrayList<>(peakIndices.size() - 1);
        for (int i = 1; i < peakIndices.size(); i++) {
            double intervalMs = (peakIndices.get(i) - peakIndices.get(i - 1)) / sampleRateHz * 1000.0;
            rri.add(intervalMs);
        }
        return rri;
    }

    // =========================================================================
    // Zoom
    // =========================================================================

    private void adjustXRange(double factor) {
        // factor < 0 = reset. Apply to all linked x-axes.
        for (var chart : List.of(ecgChart, icgChart, imuAccChart)) {
            if (chart.getXAxis() instanceof javafx.scene.chart.NumberAxis axis) {
                if (factor < 0) {
                    axis.setAutoRanging(true);
                } else {
                    axis.setAutoRanging(false);
                    double range = axis.getUpperBound() - axis.getLowerBound();
                    double centre = axis.getLowerBound() + range / 2.0;
                    double newHalf = (range * factor) / 2.0;
                    axis.setLowerBound(Math.max(0, centre - newHalf));
                    axis.setUpperBound(centre + newHalf);
                }
            }
        }
    }

    // =========================================================================
    // CSV export
    // =========================================================================

    private void exportCsv(File dest) {
        try (PrintWriter pw = new PrintWriter(new FileWriter(dest))) {
            // HRV section
            pw.println("# HRV Metrics");
            pw.println("metric,value,unit");
            for (HrvRow row : hrvTable.getItems()) {
                pw.printf("%s,%.4f,%s%n", row.metric(), row.value(), row.unit());
            }

            // ICG section
            pw.println();
            pw.println("# ICG / Haemodynamics");
            pw.println("beat,pep_ms,lvet_ms,sv_ml,co_lpm,z0_ohm");
            for (IcgRow row : icgTable.getItems()) {
                pw.printf("%d,%.2f,%.2f,%.2f,%.3f,%.3f%n",
                        row.beat(), row.pepMs(), row.lvetMs(), row.svMl(), row.coLpm(), row.z0());
            }

            // RRI section
            pw.println();
            pw.println("# RR Intervals");
            pw.println("beat,rri_ms");
            for (int i = 0; i < rriMs.size(); i++) {
                pw.printf("%d,%.2f%n", i + 1, rriMs.get(i));
            }

            setStatus("Exported to " + dest.getName());
        } catch (IOException ex) {
            showError("Export failed", ex.getMessage());
        }
    }

    // =========================================================================
    // UI utilities
    // =========================================================================

    private void enablePostLoadMenus() {
        menuExportCsv.setDisable(false);
        menuDetectRpeaks.setDisable(false);
        menuDetectBpoints.setDisable(false);
        if (menuComputeCsi != null) menuComputeCsi.setDisable(false);
    }

    private void setStatus(String msg) {
        if (Platform.isFxApplicationThread()) {
            statusLabel.setText(msg);
        } else {
            Platform.runLater(() -> statusLabel.setText(msg));
        }
    }

    private void showError(String title, String msg) {
        Alert a = new Alert(Alert.AlertType.ERROR);
        a.setTitle(title);
        a.setHeaderText(title);
        a.setContentText(msg);
        a.showAndWait();
    }

    private void showInfo(String title, String msg) {
        Alert a = new Alert(Alert.AlertType.INFORMATION);
        a.setTitle(title);
        a.setHeaderText(title);
        a.setContentText(msg);
        a.showAndWait();
    }

    private static String baseName(String filename) {
        int dot = filename.lastIndexOf('.');
        return dot >= 0 ? filename.substring(0, dot) : filename;
    }

    // =========================================================================
    // Table row types (package-private records)
    // =========================================================================

    record HrvRow(String metric, double value, String unit) {}

    record CsiRow(int epoch, double score, double pepZ, double sclZ, double rmssdZ, double scrZ) {}

    record IcgRow(int beat, double pepMs, double lvetMs, double svMl, double coLpm, double z0) {}

    /**
     * One row in the Activiteit table, merging one HAR epoch with its
     * corresponding SAD epoch (if available).
     *
     * @param timeLabel       human-readable epoch start time (mm:ss)
     * @param activityClass   HAR activity class name
     * @param cadenceSpm      step cadence [steps/min]; NaN if not applicable
     * @param motionIntensity normalised motion intensity 0–1
     * @param speakingPct     speaking fraction × 100 [%]; NaN if SAD not available
     * @param altitudeChangeM barometric altitude change over epoch [m]; NaN if no baro
     */
    record ActivityRow(
            String timeLabel,
            String activityClass,
            float  cadenceSpm,
            float  motionIntensity,
            float  speakingPct,
            float  altitudeChangeM
    ) {}
}
