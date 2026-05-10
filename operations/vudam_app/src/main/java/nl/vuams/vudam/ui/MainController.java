package nl.vuams.vudam.ui;

import javafx.application.Platform;
import javafx.beans.property.SimpleDoubleProperty;
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
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.stage.FileChooser;
import javafx.stage.Stage;
import nl.vuams.vudam.analysis.HRVAnalyser;
import nl.vuams.vudam.analysis.RPeakDetector;
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

    // ---- Menu items that need enabling after load ----
    @FXML private MenuItem menuExportCsv;
    @FXML private MenuItem menuDetectRpeaks;
    @FXML private MenuItem menuComputeHrv;
    @FXML private MenuItem menuDetectBpoints;

    // ---- Status bar ----
    @FXML private Label statusLabel;

    // ---- State ----
    private Stage primaryStage;
    private List<DataBlock> loadedBlocks = List.of();
    private List<Double>    rriMs        = List.of();  // R-R intervals in ms
    private File            currentFile;

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

                Platform.runLater(() -> {
                    loadedBlocks = blocks;
                    populateWaveforms(blocks);
                    enablePostLoadMenus();
                    setStatus("Loaded %s — %d blocks.".formatted(path.getFileName(), blocks.size()));
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

    record IcgRow(int beat, double pepMs, double lvetMs, double svMl, double coLpm, double z0) {}
}
