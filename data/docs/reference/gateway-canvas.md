# Gateway Canvas Schema (v0.0.4)

Canvas state is a JSON object stored per session and rendered by compatible UIs.

## Shape

```json
{
  "view": "dashboard",
  "widgets": [
    { "type": "card", "title": "Status", "body": "All systems nominal." },
    { "type": "metric", "label": "Latency", "value": "215ms" },
    { "type": "list", "items": ["Task A", "Task B"] },
    { "type": "progress", "percent": 42 },
    { "type": "text", "text": "Next steps ready." },
    { "type": "code", "code": "deploy --plan" }
  ]
}
```

## Widget Types

- `card`: `title` + `body`
- `metric`: `label` + `value`
- `list`: `items` array
- `progress`: `percent` (0-100)
- `text`: `text` body
- `code`: `code` block

## Validation

Use `POST /canvas/{session_id}/validate` to validate a payload before saving.
