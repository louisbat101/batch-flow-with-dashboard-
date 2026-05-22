package com.batchloader.app;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiNetworkSpecifier;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.WindowManager;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.google.android.material.button.MaterialButton;

/**
 * Main Activity – Auto-connects to the ESP32 "BatchLoader" WiFi AP
 * and loads the web UI in a full-screen WebView.
 */
public class MainActivity extends AppCompatActivity {

    private static final String ESP_SSID     = "BatchLoader";
    private static final String ESP_PASSWORD = "batch1234";
    private static final String ESP_URL      = "http://192.168.4.1";
    private static final int    PERM_REQ     = 100;

    private WebView        webView;
    private View           overlay;
    private ProgressBar    spinner;
    private TextView       statusText;
    private TextView       subText;
    private MaterialButton btnRetry;

    private Handler  handler = new Handler(Looper.getMainLooper());
    private boolean  webLoaded = false;
    private ConnectivityManager connMgr;
    private ConnectivityManager.NetworkCallback networkCallback;

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

        connMgr = (ConnectivityManager) getSystemService(CONNECTIVITY_SERVICE);

        setupWebView();

        btnRetry.setOnClickListener(v -> {
            btnRetry.setVisibility(View.GONE);
            spinner.setVisibility(View.VISIBLE);
            startConnection();
        });

        // Check permissions, then connect
        if (hasPermissions()) {
            startConnection();
        } else {
            requestPerms();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (networkCallback != null && connMgr != null) {
            try { connMgr.unregisterNetworkCallback(networkCallback); } catch (Exception ignored) {}
        }
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
        // Clear all cached data so we always get fresh files from ESP32
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

            @Override
            public void onReceivedError(WebView view, WebResourceRequest req, WebResourceError err) {
                super.onReceivedError(view, req, err);
                if (req.isForMainFrame()) {
                    showError("Cannot reach Batch Loader", "Retrying…");
                    handler.postDelayed(() -> webView.loadUrl(ESP_URL), 3000);
                }
            }
        });
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  WiFi Auto-Connect
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    private void startConnection() {
        setStatus("Connecting to Batch Loader…", "Looking for BatchLoader WiFi…");

        // Check if already on the right network
        if (isOnEspWifi()) {
            setStatus("Connected!", "Loading interface…");
            handler.postDelayed(() -> webView.loadUrl(ESP_URL), 500);
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            connectWifiModern();
        } else {
            connectWifiLegacy();
        }
    }

    /**
     * Android 10+ : use WifiNetworkSpecifier (no saved network needed)
     */
    private void connectWifiModern() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return;

        setStatus("Connecting to BatchLoader…", "Requesting WiFi connection…");

        WifiNetworkSpecifier specifier = new WifiNetworkSpecifier.Builder()
                .setSsid(ESP_SSID)
                .setWpa2Passphrase(ESP_PASSWORD)
                .build();

        NetworkRequest request = new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .setNetworkSpecifier(specifier)
                .build();

        networkCallback = new ConnectivityManager.NetworkCallback() {
            @Override
            public void onAvailable(@NonNull Network network) {
                // Bind this process to use the ESP32 WiFi network
                connMgr.bindProcessToNetwork(network);
                handler.post(() -> {
                    setStatus("Connected!", "Loading interface…");
                    handler.postDelayed(() -> webView.loadUrl(ESP_URL), 800);
                });
            }

            @Override
            public void onUnavailable() {
                handler.post(() -> showError("Could not connect to BatchLoader WiFi",
                        "Make sure the ESP32 is powered on"));
            }

            @Override
            public void onLost(@NonNull Network network) {
                handler.post(() -> {
                    if (!webLoaded) {
                        showError("WiFi connection lost", "Retrying…");
                        handler.postDelayed(() -> startConnection(), 2000);
                    }
                });
            }
        };

        connMgr.requestNetwork(request, networkCallback);
    }

    /**
     * Android 9 and below: use old WifiManager API
     */
    @SuppressWarnings("deprecation")
    private void connectWifiLegacy() {
        setStatus("Connecting to BatchLoader…", "Adding WiFi network…");

        WifiManager wifiMgr = (WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE);
        if (!wifiMgr.isWifiEnabled()) {
            wifiMgr.setWifiEnabled(true);
        }

        WifiConfiguration conf = new WifiConfiguration();
        conf.SSID = "\"" + ESP_SSID + "\"";
        conf.preSharedKey = "\"" + ESP_PASSWORD + "\"";

        int netId = wifiMgr.addNetwork(conf);
        if (netId == -1) {
            // Maybe already saved
            for (WifiConfiguration existing : wifiMgr.getConfiguredNetworks()) {
                if (existing.SSID != null && existing.SSID.equals("\"" + ESP_SSID + "\"")) {
                    netId = existing.networkId;
                    break;
                }
            }
        }

        if (netId != -1) {
            wifiMgr.disconnect();
            wifiMgr.enableNetwork(netId, true);
            wifiMgr.reconnect();

            // Wait a bit then try loading
            setStatus("Connecting…", "Waiting for WiFi…");
            handler.postDelayed(() -> {
                if (isOnEspWifi()) {
                    setStatus("Connected!", "Loading interface…");
                    handler.postDelayed(() -> webView.loadUrl(ESP_URL), 500);
                } else {
                    showError("Could not connect to BatchLoader WiFi",
                            "Make sure the ESP32 is powered on");
                }
            }, 5000);
        } else {
            showError("Failed to add WiFi network", "Check your WiFi settings");
        }
    }

    @SuppressWarnings("deprecation")
    private boolean isOnEspWifi() {
        WifiManager wifiMgr = (WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE);
        WifiInfo info = wifiMgr.getConnectionInfo();
        if (info != null && info.getSSID() != null) {
            String ssid = info.getSSID().replace("\"", "");
            return ESP_SSID.equals(ssid);
        }
        return false;
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  UI Helpers
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    private void showWebView() {
        overlay.setVisibility(View.GONE);
        webView.setVisibility(View.VISIBLE);
    }

    private void setStatus(String title, String sub) {
        statusText.setText(title);
        subText.setText(sub);
        spinner.setVisibility(View.VISIBLE);
        btnRetry.setVisibility(View.GONE);
    }

    private void showError(String title, String sub) {
        statusText.setText(title);
        subText.setText(sub);
        spinner.setVisibility(View.GONE);
        btnRetry.setVisibility(View.VISIBLE);
        overlay.setVisibility(View.VISIBLE);
        webView.setVisibility(View.GONE);
        webLoaded = false;
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  Permissions
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    private boolean hasPermissions() {
        if (Build.VERSION.SDK_INT >= 33) {
            return ContextCompat.checkSelfPermission(this,
                    Manifest.permission.NEARBY_WIFI_DEVICES) == PackageManager.PERMISSION_GRANTED;
        }
        return ContextCompat.checkSelfPermission(this,
                Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestPerms() {
        if (Build.VERSION.SDK_INT >= 33) {
            ActivityCompat.requestPermissions(this,
                    new String[]{ Manifest.permission.NEARBY_WIFI_DEVICES }, PERM_REQ);
        } else {
            ActivityCompat.requestPermissions(this,
                    new String[]{ Manifest.permission.ACCESS_FINE_LOCATION }, PERM_REQ);
        }
    }

    @Override
    public void onRequestPermissionsResult(int reqCode, @NonNull String[] perms, @NonNull int[] results) {
        super.onRequestPermissionsResult(reqCode, perms, results);
        if (reqCode == PERM_REQ) {
            if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
                startConnection();
            } else {
                showError("WiFi permission required",
                        "Please grant permission in Settings");
            }
        }
    }
}
