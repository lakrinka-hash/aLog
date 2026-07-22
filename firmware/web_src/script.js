// Main tabs
function switchTab(tabId) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(el => el.classList.remove('active'));
    
    if (tabId === 'dashboard') {
        document.getElementById('dashboard-tab').classList.add('active');
        document.querySelector('button[onclick="switchTab(\'dashboard\')"]').classList.add('active');
    } else if (tabId === 'settings') {
        document.getElementById('settings-tab').classList.add('active');
        document.querySelector('button[onclick="switchTab(\'settings\')"]').classList.add('active');
        loadSettings();
    }
}

// Sub-tabs in Settings Sidebar (easily extensible!)
function switchSettingsSection(sectionId) {
    document.querySelectorAll('.settings-section').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.settings-nav-btn').forEach(el => el.classList.remove('active'));
    
    document.getElementById('sect-' + sectionId).classList.add('active');
    
    const btn = document.querySelector(`.settings-nav-btn[onclick*="${sectionId}"]`);
    if (btn) {
        btn.classList.add('active');
    }
}

function updateData() {
    fetch('/api/data')
        .then(r => r.json())
        .then(data => {
            document.getElementById('sys-time').innerText = 'Internal RTC: ' + data.time;
            document.getElementById('uptime').innerText = data.uptime;
            document.getElementById('wifi-status').innerText = 'ONLINE';
            document.getElementById('wifi-status').style.backgroundColor = '#38a169';
            
            if (data.voltage_err) {
                document.getElementById('val-voltage').innerHTML = '<span class="error">ERROR</span>';
            } else {
                document.getElementById('val-voltage').innerHTML = data.voltage.toFixed(4) + '<span class="unit">V</span>';
            }

            if (data.current_err) {
                document.getElementById('val-current').innerHTML = '<span class="error">ERROR</span>';
            } else {
                document.getElementById('val-current').innerHTML = data.current.toFixed(4) + '<span class="unit">A</span>';
            }

            if (data.voltage_err || data.current_err) {
                document.getElementById('val-power').innerHTML = '<span class="error">ERROR</span>';
            } else {
                document.getElementById('val-power').innerHTML = (data.voltage * data.current).toFixed(4) + '<span class="unit">W</span>';
            }

            if (data.sd_err) {
                document.getElementById('val-sd').innerHTML = '<span class="error">DISCONNECTED</span>';
            } else if (data.sd_overflow) {
                document.getElementById('val-sd').innerHTML = '<span class="error">CARD FULL</span>';
            } else {
                document.getElementById('val-sd').innerHTML = 
                    'Free: ' + data.sd_free + ' Mb<br><span style="font-size:12px;color:#718096">Total Capacity: ' + data.sd_total + ' Mb</span>';
            }

            document.getElementById('relay1').checked = data.relay1;
            document.getElementById('relay2').checked = data.relay2;
        })
        .catch(err => {
            console.error("Polling error:", err);
            document.getElementById('wifi-status').innerText = 'OFFLINE';
            document.getElementById('wifi-status').style.backgroundColor = '#e53e3e';
        });
}

function toggleRelay(id, state) {
    fetch('/api/relay?id=' + id + '&state=' + (state ? 1 : 0), { method: 'POST' })
        .then(r => r.json())
        .then(data => {
            console.log('Relay ' + id + ' set to ' + state);
        })
        .catch(err => console.error("Toggle error:", err));
}

function renderWifiList(wifiAps) {
    const container = document.getElementById('wifi-list-container');
    if (!wifiAps || wifiAps.length === 0) {
        container.innerHTML = '<p style="font-size:13px; color:#718096; text-align:center; padding: 10px; margin:0;">No Wi-Fi profiles configured. Please add one below.</p>';
        return;
    }

    let html = `
        <table class="mock-table" style="margin-top: 0;">
            <thead>
                <tr>
                    <th>SSID</th>
                    <th style="width: 80px; text-align: right;">Action</th>
                </tr>
            </thead>
            <tbody>
    `;

    wifiAps.forEach(ap => {
        html += `
            <tr>
                <td style="font-weight: 500;">📶 ${escapeHtml(ap.ssid)}</td>
                <td style="text-align: right;">
                    <button class="btn-sm btn-danger-light" onclick="deleteWifiNetwork('${escapeHtml(ap.ssid)}')">Delete</button>
                </td>
            </tr>
        `;
    });

    html += `
            </tbody>
        </table>
    `;
    container.innerHTML = html;
}

function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#039;');
}

function loadSettings() {
    fetch('/api/settings')
        .then(r => r.json())
        .then(data => {
            document.getElementById('udp_ip').value = data.udp_ip;
            document.getElementById('udp_port').value = data.udp_port;
            document.getElementById('voltage_coef').value = data.voltage_coef !== undefined ? data.voltage_coef : 1.0;
            document.getElementById('voltage_offset').value = data.voltage_offset !== undefined ? data.voltage_offset : 0.0;
            document.getElementById('current_coef').value = data.current_coef !== undefined ? data.current_coef : 1.0;
            document.getElementById('current_offset').value = data.current_offset !== undefined ? data.current_offset : 0.0;
            renderWifiList(data.wifi_aps);
            // Also trigger system status load
            loadSystemStatus();
        })
        .catch(err => {
            console.error("Failed to load settings:", err);
            showSettingsAlert("Failed to load settings from device", "error");
        });
}

function saveAdcCalibration(e) {
    e.preventDefault();
    const v_coef = document.getElementById('voltage_coef').value;
    const v_offset = document.getElementById('voltage_offset').value;
    const i_coef = document.getElementById('current_coef').value;
    const i_offset = document.getElementById('current_offset').value;
    
    const params = new URLSearchParams();
    params.append('voltage_coef', v_coef);
    params.append('voltage_offset', v_offset);
    params.append('current_coef', i_coef);
    params.append('current_offset', i_offset);

    showSettingsAlert("Saving calibration settings...", "info");

    fetch('/api/settings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: params.toString()
    })
    .then(r => r.json())
    .then(data => {
        if (data.status === 'ok') {
            showSettingsAlert("Calibration settings saved and applied successfully!", "success");
            setTimeout(() => {
                const alert = document.getElementById('settings-alert');
                alert.style.display = "none";
            }, 4000);
        } else {
            showSettingsAlert("Device returned error while saving calibration", "error");
        }
    })
    .catch(err => {
        console.error("Failed to save calibration:", err);
        showSettingsAlert("Failed to connect to device to save calibration settings", "error");
    });
}

function addWifiNetwork(e) {
    e.preventDefault();
    const ssid = document.getElementById('wifi_ssid').value;
    const pass = document.getElementById('wifi_pass').value;

    const params = new URLSearchParams();
    params.append('ssid', ssid);
    params.append('password', pass);

    showSettingsAlert("Saving network profile and reconnecting...", "info");

    fetch('/api/wifi/add', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: params.toString()
    })
    .then(r => r.json())
    .then(data => {
        if (data.status === 'ok') {
            showSettingsAlert("Network saved! The logger is reconnecting to Wi-Fi...", "success");
            document.getElementById('wifi_ssid').value = '';
            document.getElementById('wifi_pass').value = '';
            // Reload list after short delay
            setTimeout(loadSettings, 1000);
        } else {
            showSettingsAlert("Device returned error while saving network", "error");
        }
    })
    .catch(err => {
        console.error("Failed to add network:", err);
        showSettingsAlert("Failed to save network configuration", "error");
    });
}

function deleteWifiNetwork(ssid) {
    if (!confirm(`Are you sure you want to delete network "${ssid}"?`)) {
        return;
    }

    const params = new URLSearchParams();
    params.append('ssid', ssid);

    showSettingsAlert("Deleting network profile...", "info");

    fetch('/api/wifi/delete', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: params.toString()
    })
    .then(r => r.json())
    .then(data => {
        if (data.status === 'ok') {
            showSettingsAlert(`Deleted network "${ssid}" successfully. Reconnecting...`, "success");
            setTimeout(loadSettings, 1000);
        } else {
            showSettingsAlert("Device returned error while deleting", "error");
        }
    })
    .catch(err => {
        console.error("Failed to delete network:", err);
        showSettingsAlert("Failed to delete network configuration", "error");
    });
}

function saveSettings(e) {
    e.preventDefault();
    const ip = document.getElementById('udp_ip').value;
    const port = document.getElementById('udp_port').value;
    
    const params = new URLSearchParams();
    params.append('udp_ip', ip);
    params.append('udp_port', port);

    showSettingsAlert("Saving settings...", "info");

    fetch('/api/settings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: params.toString()
    })
    .then(r => r.json())
    .then(data => {
        if (data.status === 'ok') {
            showSettingsAlert("Settings saved and applied successfully!", "success");
            setTimeout(() => {
                const alert = document.getElementById('settings-alert');
                alert.style.display = "none";
            }, 4000);
        } else {
            showSettingsAlert("Device returned error while saving", "error");
        }
    })
    .catch(err => {
        console.error("Failed to save settings:", err);
        showSettingsAlert("Failed to connect to device to save settings", "error");
    });
}

function showSettingsAlert(msg, type) {
    const alert = document.getElementById('settings-alert');
    alert.innerText = msg;
    alert.className = "alert";
    alert.style.display = "block";
    if (type === 'success') {
        alert.classList.add('alert-success');
    } else if (type === 'error') {
        alert.classList.add('alert-error');
    } else {
        // Info/pending state
        alert.style.backgroundColor = "#ebf8ff";
        alert.style.color = "#2b6cb0";
        alert.style.borderColor = "#bee3f8";
        alert.style.borderStyle = "solid";
        alert.style.borderWidth = "1px";
    }
}

function loadSystemStatus() {
    fetch('/api/system/status')
        .then(r => r.json())
        .then(data => {
            document.getElementById('sys-idf-ver').innerText = data.idf_version;
            document.getElementById('sys-chip').innerText = data.chip_model + " (Rev " + data.chip_revision + ", " + data.chip_cores + " CPU Core)";
            
            const freeHeapKb = (data.free_heap / 1024).toFixed(1);
            const minHeapKb = (data.min_free_heap / 1024).toFixed(1);
            document.getElementById('sys-heap').innerText = freeHeapKb + " KB (Min: " + minHeapKb + " KB)";
            
            if (data.spiffs_err) {
                document.getElementById('sys-spiffs').innerHTML = '<span class="error">SPIFFS ERROR</span>';
            } else {
                const totalKb = (data.spiffs_total / 1024).toFixed(1);
                const usedKb = (data.spiffs_used / 1024).toFixed(1);
                const percent = ((data.spiffs_used / data.spiffs_total) * 100).toFixed(1);
                document.getElementById('sys-spiffs').innerText = usedKb + " KB / " + totalKb + " KB (" + percent + "%)";
            }
        })
        .catch(err => {
            console.error("Failed to load system status:", err);
            document.getElementById('sys-idf-ver').innerText = "N/A";
            document.getElementById('sys-chip').innerText = "N/A";
            document.getElementById('sys-heap').innerText = "N/A";
            document.getElementById('sys-spiffs').innerText = "N/A";
        });
}

function triggerReboot() {
    if (!confirm("Are you sure you want to reboot the device?")) {
        return;
    }
    showSettingsAlert("Rebooting device. Please wait...", "info");
    fetch('/api/system/reboot', { method: 'POST' })
        .then(r => r.json())
        .then(data => {
            if (data.status === 'ok') {
                showSettingsAlert("Reboot command sent. Reconnecting in 5 seconds...", "success");
                setTimeout(() => {
                    location.reload();
                }, 5000);
            } else {
                showSettingsAlert("Device returned error for reboot", "error");
            }
        })
        .catch(err => {
            console.error("Reboot failed:", err);
            showSettingsAlert("Reboot command sent. Reconnecting in 5 seconds...", "success");
            setTimeout(() => {
                location.reload();
            }, 5000);
        });
}

function triggerFactoryReset() {
    if (!confirm("CRITICAL WARNING: This will completely erase all stored Wi-Fi configurations and calibration parameters. Are you sure you want to proceed?")) {
        return;
    }
    showSettingsAlert("Performing factory reset. Clearing storage...", "info");
    fetch('/api/system/factory-reset', { method: 'POST' })
        .then(r => r.json())
        .then(data => {
            if (data.status === 'ok') {
                showSettingsAlert("Configuration successfully cleared. Reconnecting in 6 seconds...", "success");
                setTimeout(() => {
                    location.reload();
                }, 6000);
            } else {
                showSettingsAlert("Failed to perform factory reset: " + (data.message || "Unknown error"), "error");
            }
        })
        .catch(err => {
            console.error("Factory reset failed:", err);
            showSettingsAlert("Factory reset request sent. Reconnecting in 6 seconds...", "success");
            setTimeout(() => {
                location.reload();
            }, 6000);
        });
}

setInterval(updateData, 1500);
updateData();
