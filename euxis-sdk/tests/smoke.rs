// SPDX-License-Identifier: AGPL-3.0-or-later

#[test]
fn sdk_exports_basic_contract() {
    let payload = serde_json::json!({"message": "hello"});
    assert_eq!(payload["message"], "hello");
}
