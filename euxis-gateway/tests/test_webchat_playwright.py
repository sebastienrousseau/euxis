import os
import re
import time

import pytest

sync_playwright = pytest.importorskip("playwright.sync_api").sync_playwright
from playwright.sync_api import expect


@pytest.mark.e2e
def test_webchat_smoke():
    base_url = os.environ.get("EUXIS_GATEWAY_URL")
    if not base_url:
        pytest.skip("EUXIS_GATEWAY_URL not set")

    session_id = os.environ.get("EUXIS_WEBCHAT_SESSION", "sess_webchat_demo")
    token = os.environ.get("EUXIS_GATEWAY_TOKEN", "")

    with sync_playwright() as p:
        try:
            browser = p.chromium.launch()
        except Exception as exc:  # pragma: no cover - depends on local Playwright install
            pytest.skip(f"Playwright browser not available: {exc}")
        page = browser.new_page()
        token_query = f"?token={token}" if token else ""
        page.goto(f"{base_url.rstrip('/')}/webchat/{token_query}", wait_until="domcontentloaded")

        expect(page).to_have_title("Euxis Gateway WebChat")
        expect(page.get_by_text("Transcript")).to_be_visible()
        expect(page.get_by_text("Canvas State")).to_be_visible()

        session_input = page.locator("#session")
        session_input.fill(session_id)

        editor = page.locator("#canvas-editor")
        editor.fill('{"view":"dashboard","widgets":[{"type":"card","title":"Status","body":"OK"}]}')
        page.locator("#canvas-validate").click()
        status = page.locator("#canvas-status")
        expect(status).to_contain_text(re.compile(r"Valid|Invalid", re.IGNORECASE))

        browser.close()


@pytest.mark.e2e
def test_webchat_auth_flow():
    base_url = os.environ.get("EUXIS_GATEWAY_URL")
    if not base_url:
        pytest.skip("EUXIS_GATEWAY_URL not set")

    token = os.environ.get("EUXIS_GATEWAY_TOKEN")
    if not token:
        pytest.skip("EUXIS_GATEWAY_TOKEN not set")
    session_id = os.environ.get("EUXIS_WEBCHAT_SESSION", "sess_webchat_demo")

    with sync_playwright() as p:
        try:
            browser = p.chromium.launch()
        except Exception as exc:  # pragma: no cover - depends on local Playwright install
            pytest.skip(f"Playwright browser not available: {exc}")
        page = browser.new_page()
        page.goto(f"{base_url.rstrip('/')}/webchat/?token={token}&e2e=1", wait_until="domcontentloaded")

        expect(page.get_by_text("Transcript")).to_be_visible()

        with page.expect_response(re.compile(r"/sessions$")) as sessions_resp:
            page.locator("#refresh-sessions").click()
        assert sessions_resp.value.status == 200

        with page.expect_response(re.compile(r"/approvals$")) as approvals_resp:
            page.locator("#refresh-approvals").click()
        assert approvals_resp.value.status == 200

        unique_content = f"request approval {int(time.time() * 1000)}"
        payload = {
            "type": "request",
            "id": "req_e2e_approval",
            "method": "chat.send",
            "params": {
                "session_id": session_id,
                "channel_id": "webchat",
                "role": "user",
                "content": unique_content,
                "meta": {"agent": "architect", "approved": False, "elevated": "full"},
            },
        }

        sent = page.evaluate("payload => window.euxisWebchat.sendPayload(payload)", payload)
        assert sent is True

        approvals_items = page.locator("#approvals .item")
        page.locator("#refresh-approvals").click()
        target = approvals_items.filter(has_text=unique_content)
        expect(target).to_have_count(1, timeout=5000)

        with page.expect_response(re.compile(r"/approvals/.+/reject$")) as reject_resp:
            target.first.get_by_text("Reject").click()
        assert reject_resp.value.status == 200
        page.locator("#refresh-approvals").click()
        expect(target).to_have_count(0, timeout=5000)

        browser.close()
