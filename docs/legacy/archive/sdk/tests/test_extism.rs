use euxis_sdk::Error;
use euxis_sdk::{MCPResult, MCPContent};
use euxis_sdk::{call_tool, sign_payload, get_metrics};
use euxis_sdk::euxis_sdk::platform::extism::mock;

fn set_mock_response(result: &MCPResult) {
    let bytes = rmp_serde::to_vec_named(result).unwrap();
    mock::MOCK_RESPONSE.with(|m| *m.borrow_mut() = Some(bytes));
}

fn set_raw_mock_response(bytes: &[u8]) {
    mock::MOCK_RESPONSE.with(|m| *m.borrow_mut() = Some(bytes.to_vec()));
}

fn text_result(value: &str) -> MCPResult {
    MCPResult {
        content: Some(vec![MCPContent {
            content_type: "text".to_string(),
            text: value.to_string(),
        }]),
        error: None,
    }
}

#[test]
fn call_tool_deserialization_error() {
    set_raw_mock_response(&[0xFF, 0xFF]);
    let result = call_tool("test", serde_json::json!({}));
    assert!(matches!(result, Err(Error::Deserialization(_))));
}

#[test]
fn call_tool_success() {
    set_mock_response(&text_result("success"));
    let result = call_tool("test", serde_json::json!({})).unwrap();
    assert_eq!(result.content.unwrap()[0].text, "success");
}

#[test]
fn test_public_sign_payload() {
    set_mock_response(&text_result("native-signature"));
    let signature = sign_payload("payload_data").unwrap();
    assert_eq!(signature, "native-signature");
}

#[test]
fn test_public_get_metrics() {
    set_mock_response(&text_result("{\"cpu\": 0.12, \"mem\": 42}"));
    let metrics = get_metrics().unwrap();
    assert_eq!(metrics["cpu"], 0.12);
    assert_eq!(metrics["mem"], 42);
}

#[test]
fn test_first_content_text_error() {
    let res = MCPResult { content: None, error: Some("failed".to_string()) };
    assert_eq!(mock::mock_first_content_text(res, "op"), Err(Error::Operation("failed".to_string())));

    let res_empty_vec = MCPResult { content: Some(vec![]), error: None };
    assert_eq!(mock::mock_first_content_text(res_empty_vec, "op"), Err(Error::EmptyResponse("op".to_string())));
}

#[test]
fn sign_payload_internal_success() {
    // Tests signature natively using the mock FFI interface since sign_payload_internal was removed
    set_mock_response(&text_result("signed-payload"));
    let signature = euxis_sdk::sign_payload("data").unwrap();
    assert_eq!(signature, "signed-payload");
}

#[test]
fn test_public_get_metrics_json_error() {
    set_mock_response(&text_result("invalid-json"));
    let err = get_metrics().unwrap_err();
    assert!(matches!(err, Error::Json(_)));
}

fn set_mock_error(error: &str) {
    mock::MOCK_ERROR.with(|m| *m.borrow_mut() = Some(error.to_string()));
}

fn clear_mock_error() {
    mock::MOCK_ERROR.with(|m| *m.borrow_mut() = None);
}

#[test]
fn test_extism_ffi_failure() {
    clear_mock_error();
    set_mock_error("Simulated Extism Error");
    let result = call_tool("test", serde_json::json!({}));
    assert!(matches!(result, Err(Error::Extism(_))));
    clear_mock_error();
}

#[test]
fn test_sign_payload_ffi_failure() {
    clear_mock_error();
    set_mock_error("Simulated Extism sign error");
    let err = sign_payload("payload").unwrap_err();
    assert!(matches!(err, Error::Extism(_)));
    clear_mock_error();
}

#[test]
fn test_get_metrics_ffi_failure() {
    clear_mock_error();
    set_mock_error("Simulated Extism metric error");
    let err = get_metrics().unwrap_err();
    assert!(matches!(err, Error::Extism(_)));
    clear_mock_error();
}

#[test]
fn test_get_metrics_empty_response_error() {
    clear_mock_error();
    let res_empty = MCPResult { content: Some(vec![]), error: None };
    set_mock_response(&res_empty);
    let err = get_metrics().unwrap_err();
    assert!(matches!(err, Error::EmptyResponse(_)));
}

#[test]
fn test_sign_payload_empty_response_error() {
    clear_mock_error();
    let res_empty = MCPResult { content: Some(vec![]), error: None };
    set_mock_response(&res_empty);
    let err = sign_payload("payload_data").unwrap_err();
    assert!(matches!(err, Error::EmptyResponse(_)));
}

struct FailSerialize;

impl serde::Serialize for FailSerialize {
    fn serialize<S>(&self, _serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        Err(serde::ser::Error::custom("Simulated serialization failure"))
    }
}

#[test]
fn test_serialization_error() {
    let err = euxis_sdk::euxis_sdk::platform::extism::call_tool_with("test", &FailSerialize).unwrap_err();
    assert!(matches!(err, Error::Serialization(_)));
}
