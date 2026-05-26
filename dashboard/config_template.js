// Rename this file to config.js before running locally.
// Keep config.js out of version control.

const useTestBroker = true;

const mqttConfig = useTestBroker
    ? {
        host: "broker.hivemq.com",
        port: 8000,
        username: "",
        password: ""
    }
    : {
        host: "YOUR_BROKER_IP",
        port: 9001,
        username: "YOUR_USERNAME",
        password: "YOUR_PASSWORD"
    };

export default mqttConfig;