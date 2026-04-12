module.exports = [
  {
    type: "heading",
    defaultValue: "Solar Gradient Settings"
  },
  {
    type: "text",
    defaultValue: "Phone location powers solar angle/color. Chicago is used as fallback."
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Time Readability"
      },
      {
        type: "select",
        messageKey: "TextOverrideMode",
        label: "Text Style",
        defaultValue: "0",
        options: [
          {
            label: "Auto paired contrast",
            value: "0"
          },
          {
            label: "Pure white",
            value: "1"
          },
          {
            label: "Pure black",
            value: "2"
          },
          {
            label: "Black with glow",
            value: "3"
          }
        ]
      }
    ]
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Time Size"
      },
      {
        type: "text",
        defaultValue: "Choose the time size for your connected watch."
      },
      {
        type: "select",
        messageKey: "TimeSizeBasalt",
        label: "Time size",
        defaultValue: "1",
        capabilities: ["PLATFORM_BASALT"],
        options: [
          { label: "Compact", value: "0" },
          { label: "Balanced", value: "1" },
          { label: "Large", value: "2" }
        ]
      },
      {
        type: "select",
        messageKey: "TimeSizeChalk",
        label: "Time size",
        defaultValue: "1",
        capabilities: ["PLATFORM_CHALK"],
        options: [
          { label: "Compact", value: "0" },
          { label: "Balanced", value: "1" },
          { label: "Large", value: "2" }
        ]
      },
      {
        type: "select",
        messageKey: "TimeSizeEmery",
        label: "Time size",
        defaultValue: "1",
        capabilities: ["PLATFORM_EMERY"],
        options: [
          { label: "Compact", value: "0" },
          { label: "Balanced", value: "1" },
          { label: "Large", value: "2" }
        ]
      },
      {
        type: "select",
        messageKey: "TimeSizeGabbro",
        label: "Time size",
        defaultValue: "1",
        capabilities: ["PLATFORM_GABBRO"],
        options: [
          { label: "Compact", value: "0" },
          { label: "Balanced", value: "1" },
          { label: "Large", value: "2" }
        ]
      }
    ]
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Motion & Battery"
      },
      {
        type: "select",
        messageKey: "MotionMode",
        label: "Gradient motion",
        defaultValue: "0",
        options: [
          {
            label: "Hybrid (default)",
            value: "0"
          },
          {
            label: "Calm",
            value: "1"
          },
          {
            label: "Dynamic",
            value: "2"
          }
        ]
      },
      {
        type: "toggle",
        messageKey: "BatterySaveMode",
        label: "Battery save mode",
        defaultValue: false
      }
    ]
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Footer Visibility"
      },
      {
        type: "toggle",
        messageKey: "ShowLocation",
        label: "Show city name",
        defaultValue: true
      },
      {
        type: "toggle",
        messageKey: "ShowAltitude",
        label: "Show altitude",
        defaultValue: true
      }
    ]
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Weather (Open-Meteo)"
      },
      {
        type: "toggle",
        messageKey: "WeatherEnabled",
        label: "Enable weather",
        defaultValue: true
      },
      {
        type: "toggle",
        messageKey: "WeatherUnitFahrenheit",
        label: "Use Fahrenheit",
        defaultValue: true
      },
      {
        type: "select",
        messageKey: "WeatherDetailLevel",
        label: "Weather detail",
        defaultValue: "1",
        options: [
          { label: "Off", value: "0" },
          { label: "Basic", value: "1" },
          { label: "Expanded", value: "2" }
        ]
      }
    ]
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Location Source"
      },
      {
        type: "toggle",
        messageKey: "ForceChicagoForTesting",
        label: "Force Chicago (emulator testing)",
        defaultValue: false
      },
      {
        type: "toggle",
        messageKey: "CustomLocationEnabled",
        label: "Use custom location",
        defaultValue: false
      },
      {
        type: "input",
        messageKey: "CustomLatitude",
        label: "Latitude",
        defaultValue: "41.8781",
        attributes: {
          placeholder: "41.8781"
        }
      },
      {
        type: "input",
        messageKey: "CustomLongitude",
        label: "Longitude",
        defaultValue: "-87.6298",
        attributes: {
          placeholder: "-87.6298"
        }
      }
    ]
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Solar Dev Mode"
      },
      {
        type: "toggle",
        messageKey: "DevModeEnabled",
        label: "Enable dev time/place override",
        defaultValue: false
      },
      {
        type: "input",
        messageKey: "DevLatitude",
        label: "Dev latitude",
        defaultValue: "41.8781",
        attributes: {
          placeholder: "41.8781"
        }
      },
      {
        type: "input",
        messageKey: "DevLongitude",
        label: "Dev longitude",
        defaultValue: "-87.6298",
        attributes: {
          placeholder: "-87.6298"
        }
      },
      {
        type: "input",
        messageKey: "DevDateTime",
        label: "Dev local datetime",
        defaultValue: "",
        attributes: {
          placeholder: "2026-04-11T11:30"
        }
      },
      {
        type: "toggle",
        messageKey: "DevSweepEnabled",
        label: "Sweep altitude/azimuth",
        defaultValue: false
      },
      {
        type: "input",
        messageKey: "DevSweepCycleSeconds",
        label: "Sweep cycle seconds",
        defaultValue: "180",
        attributes: {
          placeholder: "180"
        }
      },
      {
        type: "toggle",
        messageKey: "DevShowDebugOverlay",
        label: "Show dev overlay on watch",
        defaultValue: true
      },
      {
        type: "toggle",
        messageKey: "DebugBenchmark",
        label: "Enable benchmark gradient mode",
        defaultValue: false
      }
    ]
  },
  {
    type: "submit",
    defaultValue: "Save"
  }
];
