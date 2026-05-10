package nl.vuams.vudam;

import javafx.application.Application;
import javafx.fxml.FXMLLoader;
import javafx.scene.Scene;
import javafx.scene.layout.BorderPane;
import javafx.stage.Stage;
import nl.vuams.vudam.ui.MainController;

import java.io.IOException;
import java.net.URL;
import java.util.Objects;

/**
 * VU-DAMS — VU-AMS Desktop Analysis &amp; Management System.
 *
 * <p>JavaFX application entry point. Loads {@code main.fxml}, wires the
 * {@link MainController} to the primary stage, and shows the window.
 *
 * <h2>Launch</h2>
 * <pre>{@code
 *   mvn javafx:run
 * }</pre>
 * or via the shaded fat-JAR:
 * <pre>{@code
 *   java --module-path <path-to-javafx-mods> \
 *        --add-modules javafx.controls,javafx.fxml,javafx.charts \
 *        -jar target/vudam-1.0.0-SNAPSHOT.jar
 * }</pre>
 */
public class VUDAMSApp extends Application {

    private static final String FXML_PATH = "/nl/vuams/vudam/main.fxml";
    private static final String APP_TITLE = "VU-DAMS — Offline Signal Analysis";
    private static final double MIN_WIDTH  = 900.0;
    private static final double MIN_HEIGHT = 600.0;

    @Override
    public void start(Stage primaryStage) throws IOException {
        URL fxmlUrl = getClass().getResource(FXML_PATH);
        Objects.requireNonNull(fxmlUrl,
                "FXML not found on classpath: " + FXML_PATH);

        FXMLLoader loader = new FXMLLoader(fxmlUrl);
        BorderPane root = loader.load();

        // Give controller a reference to the stage (for file dialogs etc.)
        MainController controller = loader.getController();
        controller.setPrimaryStage(primaryStage);

        Scene scene = new Scene(root, 1280, 800);

        // Load stylesheet if present (optional — fails gracefully)
        URL css = getClass().getResource("/nl/vuams/vudam/vudam.css");
        if (css != null) {
            scene.getStylesheets().add(css.toExternalForm());
        }

        primaryStage.setTitle(APP_TITLE);
        primaryStage.setMinWidth(MIN_WIDTH);
        primaryStage.setMinHeight(MIN_HEIGHT);
        primaryStage.setScene(scene);
        primaryStage.show();
    }

    /**
     * Standard main method — delegates to {@link Application#launch(String...)}.
     * Needed for fat-JAR execution where the launcher does not know this is a
     * JavaFX Application subclass.
     */
    public static void main(String[] args) {
        launch(args);
    }
}
