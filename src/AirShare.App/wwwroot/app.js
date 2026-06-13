// JavaScript bridge for AirShare Web UI
const airShareApi = {
    getDevices: async () => {
        try {
            const result = await window.chrome.webview.hostObjects.scriptBridge.GetDevices();
            return JSON.parse(result);
        } catch (e) {
            console.error("Error getting devices:", e);
            return [];
        }
    },
    startSharing: async (deviceIds) => {
        try {
            return await window.chrome.webview.hostObjects.scriptBridge.StartSharing(deviceIds.join(','));
        } catch (e) {
            console.error("Error starting sharing:", e);
            alert("JS Exception: " + e.message);
            return -1;
        }
    },
    stopSharing: async () => {
        try {
            return await window.chrome.webview.hostObjects.scriptBridge.StopSharing();
        } catch (e) {
            console.error("Error stopping sharing:", e);
            return -1;
        }
    },
    setMasterVolume: async (volume) => {
        try {
            window.chrome.webview.hostObjects.scriptBridge.SetMasterVolume(volume);
        } catch (e) {}
    },
    setDeviceVolume: async (deviceId, volume) => {
        try {
            window.chrome.webview.hostObjects.scriptBridge.SetDeviceVolume(deviceId, volume);
        } catch (e) {}
    },
    refreshDevices: async () => {
        try {
            window.chrome.webview.hostObjects.scriptBridge.RefreshDevices();
        } catch (e) {}
    }
};

window.airShareApi = airShareApi;

// App State
let selectedDevices = new Set();
let isSharing = false;

// UI Initialization
document.addEventListener('DOMContentLoaded', async () => {
    // We might not be in the WebView context if testing in browser
    if (!window.chrome?.webview) {
        console.warn("WebView2 environment not detected. Running with mock data.");
    }
    
    await refreshDevices();

    // Hook up master volume
    const masterVolume = document.getElementById('master-volume');
    const masterVolumeLabel = document.getElementById('master-volume-label');
    if (masterVolume) {
        masterVolume.addEventListener('input', (e) => {
            const vol = e.target.value;
            masterVolumeLabel.textContent = vol + '%';
            airShareApi.setMasterVolume(parseInt(vol));

            // Sync all individual sliders so they match master volume
            const individualSliders = document.querySelectorAll('.volume-slider');
            individualSliders.forEach(slider => {
                slider.value = vol;
                slider.dispatchEvent(new Event('input'));
            });
        });
    }

    // Hook up Mute All
    const btnMuteAll = document.getElementById('btn-mute-all');
    let previousMasterVolume = 65;
    let isMuted = false;

    if (btnMuteAll) {
        btnMuteAll.addEventListener('click', () => {
            if (!masterVolume) return;

            const icon = btnMuteAll.querySelector('.material-symbols-outlined');
            const text = btnMuteAll.querySelector('.font-bold');

            if (!isMuted) {
                // Mute
                previousMasterVolume = masterVolume.value;
                masterVolume.value = 0;
                
                if (icon) icon.textContent = 'volume_up';
                if (text) text.textContent = 'Unmute All';
                isMuted = true;
            } else {
                // Unmute
                masterVolume.value = previousMasterVolume;
                
                if (icon) icon.textContent = 'volume_off';
                if (text) text.textContent = 'Mute All';
                isMuted = false;
            }
            masterVolume.dispatchEvent(new Event('input'));
        });

        if (masterVolume) {
            masterVolume.addEventListener('input', (e) => {
                // Break mute state if user manually drags slider
                if (isMuted && e.target.value > 0) {
                    isMuted = false;
                    const icon = btnMuteAll.querySelector('.material-symbols-outlined');
                    const text = btnMuteAll.querySelector('.font-bold');
                    if (icon) icon.textContent = 'volume_off';
                    if (text) text.textContent = 'Mute All';
                }
            });
        }
    }

    // Hook up refresh button
    const btnRefresh = document.getElementById('btn-refresh');
    if (btnRefresh) {
        btnRefresh.addEventListener('click', async () => {
            // Add a spinning animation class
            const icon = btnRefresh.querySelector('span');
            if (icon) icon.classList.add('animate-spin');
            
            await airShareApi.refreshDevices();
            await refreshDevices();
            
            if (icon) setTimeout(() => icon.classList.remove('animate-spin'), 500);
        });
    }

    // Hook up buttons
    const btnStart = document.getElementById('btn-start-sharing');
    const btnStop = document.getElementById('btn-stop-sharing');
    
    if (btnStart) {
        btnStart.addEventListener('click', async () => {
            if (selectedDevices.size < 2) {
                alert("Please select at least 2 devices to share audio.");
                return;
            }
            const result = await airShareApi.startSharing(Array.from(selectedDevices));
            if (result === 0) { // Assuming 0 is success
                isSharing = true;
                updateSharingUI();
            } else {
                const errorMsg = await window.chrome.webview.hostObjects.scriptBridge.GetLastError();
                alert("Failed to start audio engine: " + errorMsg + " (Error Code: " + result + ")");
            }
        });
    }

    if (btnStop) {
        btnStop.addEventListener('click', async () => {
            await airShareApi.stopSharing();
            isSharing = false;
            updateSharingUI();
        });
    }
});

async function refreshDevices() {
    const devicesContainer = document.getElementById('devices-container');
    if (!devicesContainer) return;
    
    devicesContainer.innerHTML = '<div class="col-span-3 text-center py-4">Loading devices...</div>';
    
    const devices = await airShareApi.getDevices();
    devicesContainer.innerHTML = '';

    if (devices.length === 0) {
        devicesContainer.innerHTML = '<div class="col-span-3 text-center py-4 text-outline">No audio output devices found.</div>';
        return;
    }

    devices.forEach(device => {
        const icon = device.Type.toLowerCase().includes('earbud') ? 'earbuds' : 
                     device.Type.toLowerCase().includes('speaker') ? 'speaker' : 'headphones';
        
        // Check if device is selected
        const isSelected = selectedDevices.has(device.DeviceId);
        const cardBg = isSelected ? 'bg-primary/10 border-primary' : 'bg-white/60';
        
        const card = document.createElement('div');
        card.className = `${cardBg} backdrop-blur-md p-lg rounded-xl acrylic-stroke shadow-sm hover:shadow-md transition-all group cursor-pointer`;
        card.onclick = () => toggleDeviceSelection(device.DeviceId, card);
        
        card.innerHTML = `
            <div class="flex items-start justify-between mb-lg">
                <div class="flex items-center gap-md">
                    <div class="w-12 h-12 rounded-lg bg-surface-container-highest flex items-center justify-center text-primary">
                        <span class="material-symbols-outlined text-3xl">${icon}</span>
                    </div>
                    <div>
                        <h5 class="font-bold text-on-surface truncate w-40" title="${device.Name}">${device.Name}</h5>
                        <span class="text-xs text-secondary flex items-center gap-1">
                            <span class="w-2 h-2 rounded-full ${device.IsConnected ? 'bg-secondary' : 'bg-outline'}"></span>
                            ${device.IsConnected ? 'Connected' : 'Disconnected'}
                        </span>
                    </div>
                </div>
                <div class="flex flex-col items-end">
                    <span class="font-label-mono text-on-surface-variant text-xs">${device.LatencyMs}ms</span>
                    <input type="checkbox" class="mt-2 w-4 h-4 rounded text-primary focus:ring-primary" ${isSelected ? 'checked' : ''} onclick="event.stopPropagation(); window.toggleDeviceSelection('${device.DeviceId}', this.parentElement.parentElement.parentElement)">
                </div>
            </div>
            <div class="space-y-sm" onclick="event.stopPropagation()">
                <div class="flex items-center justify-between text-xs text-on-surface-variant">
                    <span>Output Gain</span>
                    <span class="font-label-mono volume-label">${device.Volume}%</span>
                </div>
                <input class="w-full h-1 bg-surface-container-highest rounded-lg appearance-none cursor-pointer accent-primary volume-slider" type="range" min="0" max="100" value="${device.Volume}"/>
            </div>
        `;

        const slider = card.querySelector('.volume-slider');
        const label = card.querySelector('.volume-label');
        slider.addEventListener('input', (e) => {
            const vol = e.target.value;
            label.textContent = vol + '%';
            airShareApi.setDeviceVolume(device.DeviceId, parseInt(vol));
        });

        devicesContainer.appendChild(card);
    });
}

window.toggleDeviceSelection = function(deviceId, cardElement) {
    if (selectedDevices.has(deviceId)) {
        selectedDevices.delete(deviceId);
        cardElement.classList.remove('bg-primary/10', 'border-primary');
        cardElement.classList.add('bg-white/60');
        const cb = cardElement.querySelector('input[type="checkbox"]');
        if (cb) cb.checked = false;
    } else {
        selectedDevices.add(deviceId);
        cardElement.classList.remove('bg-white/60');
        cardElement.classList.add('bg-primary/10', 'border-primary');
        const cb = cardElement.querySelector('input[type="checkbox"]');
        if (cb) cb.checked = true;
    }
}

function updateSharingUI() {
    const btnStart = document.getElementById('btn-start-sharing');
    const broadcastingCard = document.getElementById('broadcasting-card');
    const statusContainer = document.getElementById('sharing-status-container');
    const engineContainer = document.getElementById('engine-status-container');

    if (isSharing) {
        if (btnStart) btnStart.style.display = 'none';
        if (broadcastingCard) broadcastingCard.style.display = 'block';
        if (statusContainer) statusContainer.style.display = 'block';
        if (engineContainer) engineContainer.style.display = 'flex';
    } else {
        if (btnStart) btnStart.style.display = 'inline-flex';
        if (broadcastingCard) broadcastingCard.style.display = 'none';
        if (statusContainer) statusContainer.style.display = 'none';
        if (engineContainer) engineContainer.style.display = 'none';
    }
}
