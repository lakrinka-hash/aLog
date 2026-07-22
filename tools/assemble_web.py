#!/usr/bin/env python3
import os
import sys

def main():
    # Paths relative to this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    web_src_dir = os.path.join(project_root, "web_src")
    output_dir = os.path.join(project_root, "firmware", "storage")

    template_path = os.path.join(web_src_dir, "template.html")
    style_path = os.path.join(web_src_dir, "style.css")
    dashboard_path = os.path.join(web_src_dir, "dashboard.html")
    logs_path = os.path.join(web_src_dir, "logs.html")
    settings_path = os.path.join(web_src_dir, "settings.html")
    script_path = os.path.join(web_src_dir, "script.js")
    output_path = os.path.join(output_dir, "index.html")

    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)

    print("--- Web Console Assembly ---")
    print(f"Template:   {template_path}")
    print(f"Style:      {style_path}")
    print(f"Dashboard:  {dashboard_path}")
    print(f"Logs:       {logs_path}")
    print(f"Settings:   {settings_path}")
    print(f"Script:     {script_path}")
    print(f"Output to:  {output_path}")

    # Read each of the files
    try:
        with open(template_path, 'r', encoding='utf-8') as f:
            template = f.read()
        with open(style_path, 'r', encoding='utf-8') as f:
            style = f.read()
        with open(dashboard_path, 'r', encoding='utf-8') as f:
            dashboard = f.read()
        with open(logs_path, 'r', encoding='utf-8') as f:
            logs = f.read()
        with open(settings_path, 'r', encoding='utf-8') as f:
            settings = f.read()
        with open(script_path, 'r', encoding='utf-8') as f:
            script = f.read()
    except Exception as e:
        print(f"Error reading source files: {e}", file=sys.stderr)
        sys.exit(1)

    # Perform placeholder replacements
    # Identifiers:
    # /* {{STYLE_CSS}} */ -> style.css
    # <!-- {{DASHBOARD_HTML}} --> -> dashboard.html
    # <!-- {{LOGS_HTML}} --> -> logs.html
    # <!-- {{SETTINGS_HTML}} --> -> settings.html
    # // {{SCRIPT_JS}} -> script.js
    
    assembled = template
    assembled = assembled.replace("/* {{STYLE_CSS}} */", style)
    assembled = assembled.replace("<!-- {{DASHBOARD_HTML}} -->", dashboard)
    assembled = assembled.replace("<!-- {{LOGS_HTML}} -->", logs)
    assembled = assembled.replace("<!-- {{SETTINGS_HTML}} -->", settings)
    assembled = assembled.replace("// {{SCRIPT_JS}}", script)

    # Write output file
    try:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(assembled)
        print("Assembly completed successfully!")
    except Exception as e:
        print(f"Error writing output file: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
