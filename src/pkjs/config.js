module.exports = [
  {
    type: "heading",
    defaultValue: "Kodi Launch Configuration",
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Connection",
      },
      {
        type: "input",
        messageKey: "kodi_ip",
        label: "Kodi IP Address and Port",
        defaultValue: "",
      },
      {
        type: "input",
        messageKey: "kodi_user",
        label: "Username (optional)",
        defaultValue: "",
      },
      {
        type: "input",
        messageKey: "kodi_password",
        label: "Password (optional)",
        defaultValue: "",
      },
    ],
  },
  {
    type: "submit",
    defaultValue: "Save Settings",
  },
];
