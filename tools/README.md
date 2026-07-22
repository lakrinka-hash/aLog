# 🧰 Automation & Development Tools

This folder contains helper scripts and automation tools for the `aLog` project.

## 🌐 Web Console Assembly Script (`assemble_web.py`)

The modular source files of the web console are located under `web_src/` in the project root to keep the firmware directory clean and separate concerns. 

Before building the ESP-IDF firmware, the web console must be aggregated into a single, monolithic, compressed HTML file and placed in `firmware/storage/index.html` where it gets packaged into the SPIFFS storage image.

### How it Works

The script `assemble_web.py` reads:
- `web_src/template.html` (the base structure)
- `web_src/style.css` (inserted into `/* {{STYLE_CSS}} */`)
- `web_src/dashboard.html` (inserted into `<!-- {{DASHBOARD_HTML}} -->`)
- `web_src/settings.html` (inserted into `<!-- {{SETTINGS_HTML}} -->`)
- `web_src/script.js` (inserted into `// {{SCRIPT_JS}}`)

And outputs a single monolithic file to:
- `firmware/storage/index.html`

### Usage

To manually compile and update the web console inside the SPIFFS storage partition, run the script from the project root or the `tools/` directory:

```bash
python3 tools/assemble_web.py
```

### Automation

Always make sure to run this script before calling `idf.py build` if you have made any changes to the files under `web_src/`.


