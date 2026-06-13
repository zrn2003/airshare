# Stitch UI Reference

Design source: Google Stitch project **AirShare Windows Audio Sync** (`projects/15401866601213262599`).

Exported HTML screens:

- `dashboard.html` — Dashboard / sharing status
- `device-manager.html` — Device selection and management
- `analytics.html` — Latency and performance analytics
- `settings.html` — Engine and appearance settings

The WPF app implements this design using:

- Sidebar navigation shell
- Stitch color tokens (`#005FAA` primary, `#F9F9F9` surface, etc.)
- Page views under `src/AirShare.App/Views/`

To refresh exports from Stitch, re-download screen HTML via the Stitch MCP API or export from [stitch.withgoogle.com](https://stitch.withgoogle.com).
