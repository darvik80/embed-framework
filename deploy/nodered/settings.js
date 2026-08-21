/**
 * Node-RED settings for Cogitor IoT lab.
 * In this compose stack the broker hostname is "rabbitmq".
 */
module.exports = {
    uiPort: process.env.PORT || 1880,
    credentialSecret: process.env.NODERED_CREDENTIAL_SECRET || "cogitor-nodered",
    flowFile: "flows.json",
    flowFilePretty: true,
    adminAuth: undefined, // set for production
    httpAdminRoot: "/",
    httpNodeRoot: "/api",
    functionGlobalContext: {
        cogitor: {
            mqttBroker: "rabbitmq",
            mqttPort: 1883,
            eventsPrefix: "platform/v1/events",
            commandsPrefix: "platform/v1/commands",
        },
    },
    editorTheme: {
        page: {
            title: "Cogitor IoT Automation",
        },
    },
};
