// activeConfig is loaded from config.js
const BROKER_IP = activeConfig.BROKER_IP;
const BROKER_WS_PORT = activeConfig.BROKER_WS_PORT;
const USERNAME = activeConfig.USERNAME;
const PASSWORD = activeConfig.PASSWORD;
const TOPIC_STATE = "laundry/+/+/state";
const TOPIC_ALERT = "laundry/+/+/alert";

let mqttClient;
const machines = {};

function init() {
    // Generate random client ID
    const clientId = "web_" + Math.random().toString(16).substr(2, 8);

    // Create a client instance
    mqttClient = new Paho.MQTT.Client(BROKER_IP, BROKER_WS_PORT, "/mqtt", clientId);

    // set callback handlers
    mqttClient.onConnectionLost = onConnectionLost;
    mqttClient.onMessageArrived = onMessageArrived;

    // connect the client
    connectMQTT();
}

function connectMQTT() {
    const connStatus = document.getElementById("conn-status");
    const connDot = document.getElementById("conn-dot");

    connStatus.textContent = "Connecting...";
    connDot.classList.remove("connected");

    const options = {
        onSuccess: onConnect,
        onFailure: onFailure
    };
    
    if (USERNAME && PASSWORD) {
        options.userName = USERNAME;
        options.password = PASSWORD;
    }

    mqttClient.connect(options);
}

function onConnect() {
    console.log("Connected to MQTT Broker via WebSockets");
    const connStatus = document.getElementById("conn-status");
    const connDot = document.getElementById("conn-dot");

    connStatus.textContent = "Connected";
    connDot.classList.add("connected");
    
    // Also add connected to the indicator container for the green border/text
    const connIndicator = document.querySelector('.live-indicator');
    if (connIndicator) connIndicator.classList.add("connected");

    mqttClient.subscribe(TOPIC_STATE);
    mqttClient.subscribe(TOPIC_ALERT);
}

function onFailure(responseObject) {
    console.log("Failed to connect: " + responseObject.errorMessage);
    const connStatus = document.getElementById("conn-status");
    connStatus.textContent = "Connection Failed. Retrying...";

    // Retry after 5 seconds
    setTimeout(connectMQTT, 5000);
}

function onConnectionLost(responseObject) {
    if (responseObject.errorCode !== 0) {
        console.log("Connection Lost: " + responseObject.errorMessage);
        const connStatus = document.getElementById("conn-status");
        const connDot = document.getElementById("conn-dot");

        connStatus.textContent = "Disconnected";
        connDot.classList.remove("connected");
        
        const connIndicator = document.querySelector('.live-indicator');
        if (connIndicator) connIndicator.classList.remove("connected");

        // Reconnect
        setTimeout(connectMQTT, 3000);
    }
}

function onMessageArrived(message) {
    console.log("Message Arrived: " + message.payloadString);
    try {
        const data = JSON.parse(message.payloadString);
        const topicParts = message.destinationName.split('/');

        if (topicParts.length >= 4) {
            const building = topicParts[1];
            const machineId = topicParts[2];
            const key = `${building}-${machineId}`;

            if (!machines[key]) {
                machines[key] = {
                    id: machineId,
                    building: building,
                    state: "unknown",
                    type: "washer",
                    error: null,
                    health_metrics: null,
                    lastUpdated: new Date()
                };
            }
            
            machines[key].lastUpdated = new Date();
            
            if (topicParts[3] === "state") {
                machines[key].state = data.state || "unknown";
                machines[key].type = data.machine_type || machines[key].type;
                machines[key].health_metrics = data.health_metrics || null;
                
                // Clear error if returning to idle
                if (machines[key].state === "idle") {
                    machines[key].error = null;
                    machines[key].health_metrics = null;
                }
                
                // Evaluate health
                if (machines[key].state === "running" && machines[key].health_metrics) {
                    if (machines[key].type === "washer" && machines[key].health_metrics.vibration < 30) {
                        machines[key].error = "Warning: Broken Drum Detected";
                    } else if (machines[key].type === "dryer" && machines[key].health_metrics.temperature < 35) {
                        machines[key].error = "Warning: Heating Element Failure";
                    } else {
                        if (machines[key].error && machines[key].error.startsWith("Warning")) {
                            machines[key].error = null;
                        }
                    }
                }
            } else if (topicParts[3] === "alert") {
                machines[key].error = data.detail || "Error detected";
            }
            
            renderMachines();
        }
    } catch (e) {
        console.error("Failed to parse message:", e);
    }
}

function renderMachines() {
    const container = document.getElementById("machines-container");

    if (Object.keys(machines).length === 0) {
        return; // Keep loader
    }

    container.innerHTML = "";

    // Sort by ID
    const sortedKeys = Object.keys(machines).sort();

    sortedKeys.forEach(key => {
        const m = machines[key];
        const card = document.createElement("div");
        const errorClass = m.error ? "error" : "";
        card.className = `card ${m.state.toLowerCase()} ${errorClass}`;

        const timeString = m.lastUpdated.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
        const bldgName = m.building.charAt(0).toUpperCase() + m.building.slice(1);
        
        let healthUI = '';
        if (m.state === "running" && m.health_metrics) {
            let metricText = '';
            let isHealthy = !(m.error && m.error.startsWith("Warning"));
            if (m.type === "washer" && m.health_metrics.vibration !== undefined) {
                metricText = `Vib: ${m.health_metrics.vibration}Hz`;
            } else if (m.type === "dryer" && m.health_metrics.temperature !== undefined) {
                metricText = `Temp: ${m.health_metrics.temperature}°C`;
            }
            if (metricText) {
                healthUI = `
                    <div class="health-indicator ${isHealthy ? 'optimal' : 'critical'}">
                        <span class="health-dot"></span>
                        ${metricText}
                    </div>
                `;
            }
        }
        
        card.innerHTML = `
            <div class="card-header">
                <div class="machine-id">${m.id}</div>
                <div class="machine-type">${m.type}</div>
            </div>
            <div class="state-row">
                <div class="state-badge">${m.error ? 'ERROR' : m.state}</div>
                ${healthUI}
            </div>
            ${m.error ? `<div class="error-msg">${m.error}</div>` : ''}
            <div class="time-info">
                <span>Bldg: ${bldgName}</span>
                <span>Updated: ${timeString}</span>
            </div>
        `;

        container.appendChild(card);
    });
}

// Run init when DOM is loaded
document.addEventListener("DOMContentLoaded", init);
