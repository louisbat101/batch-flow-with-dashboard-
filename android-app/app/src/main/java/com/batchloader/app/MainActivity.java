package com.batchloader.app;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.WindowManager;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.button.MaterialButton;

import java.io.IOException;

/**
 * Main Activity – Runs a local web server and listens on RS-485
 * for Modbus FC04 responses from the Teensy/C3 slave system.
 * Displays the dashboard in a WebView.
 */
public class MainActivity extends AppCompatActivity {

    private static final int SERVER_PORT = 8080;
    private static final String LOCAL_URL = "http://127.0.0.1:" + SERVER_PORT;

    private WebView webView;
    private View overlay;
    private ProgressBar spinner;
    private TextView statusText;
    private TextView subText;
    private MaterialButton btnRetry;

    private Handler handler = new Handler(Looper.getMainLooper());
    private boolean webLoaded = false;

    private DashboardServer server;
    private ModbusListener modbusListener;

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  Lifecycle
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);

        webView    = findViewById(R.id.webView);
        overlay    = findViewById(R.id.connectOverlay);
        spinner    = findViewById(R.id.spinner);
        statusText = findViewById(R.id.statusText);
        subText    = findViewById(R.id.subText);
        btnRetry   = findViewById(R.id.btnRetry);

        setupWebView();

        btnRetry.setOnClickListener(v -> {
            btnRetry.setVisibility(View.GONE);
            spinner.setVisibility(View.VISIBLE);
            startServices();
        });

        startServices();
    }

    @Override
    protected void onDestroy() {
        if (modbusListener != null) modbusListener.shutdown();
        if (server != null) server.stop();
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        if (webView.getVisibility() == View.VISIBLE && webView.canGoBack()) {
            webView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  WebView setup
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    @SuppressLint("SetJavaScriptEnabled")
    private void setupWebView() {
        webView.clearCache(true);
        webView.clearHistory();

        WebSettings ws = webView.getSettings();
        ws.setJavaScriptEnabled(true);
        ws.setDomStorageEnabled(true);
        ws.setCacheMode(WebSettings.LOAD_NO_CACHE);
        ws.setUseWideViewPort(true);
        ws.setLoadWithOverviewMode(true);

        webView.setWebChromeClient(new WebChromeClient());
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                if (!webLoaded) {
                    webLoaded = true;
                    showWebView();
                }
            }
        });
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  Service Management
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    private void startServices() {
        setStatus("Starting services...", "Initializing...");

        // Step 1: Create the Modbus RS-485 listener first
        modbusListener = new ModbusListener(new ModbusListener.Listener() {
            @Override
            public void onBoardUpdated(int address, ModbusListener.BoardData data) {
                if (server != null) {
                    server.updateBoard(address, data);
                }
            }

            @Override
            public void onStatus(String msg) {
                handler.post(() -> setStatus("RS-485: " + msg, ""));
            }

            @Override
            public void onError(String msg) {
                handler.post(() -> {
                    setStatus("RS-485: " + msg, "Check wiring");
                });
            }
        });

        // Step 2: Start local web server (pass ModbusListener for valve control)
        try {
            server = new DashboardServer(modbusListener);
            server.start();
            setStatus("Server running on port " + SERVER_PORT, "Starting RS-485 listener...");
        } catch (IOException e) {
            showError("Server failed: " + e.getMessage(), "Restart the app");
            return;
        }

        // Step 3: Start RS-485 listener (Modbus master)
        if (modbusListener.hasPort()) {
            modbusListener.start();
            setStatus("RS-485 active on " + modbusListener.getPortName(), "Loading dashboard...");
        } else {
            setStatus("No RS-485 port found", "Showing demo mode");
        }

        // Step 4: Load dashboard
        handler.postDelayed(() -> webView.loadUrl(LOCAL_URL), 1000);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  UI Helpers
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    private void showWebView() {
        overlay.setVisibility(View.GONE);
        webView.setVisibility(View.VISIBLE);
    }

    private void setStatus(String title, String sub) {
        handler.post(() -> {
            statusText.setText(title);
            subText.setText(sub);
            spinner.setVisibility(View.VISIBLE);
            btnRetry.setVisibility(View.GONE);
        });
    }

    private void showError(String title, String sub) {
        handler.post(() -> {
            statusText.setText(title);
            subText.setText(sub);
            spinner.setVisibility(View.GONE);
            btnRetry.setVisibility(View.VISIBLE);
        });
    }
}
