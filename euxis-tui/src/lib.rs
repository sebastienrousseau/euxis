use pyo3::prelude::*;
use pyo3::types::PyString;
use strip_ansi_escapes::strip;

/// Quickly strips ANSI escape sequences from a string.
#[pyfunction]
fn strip_ansi<'py>(py: Python<'py>, text: &str) -> PyResult<Bound<'py, PyString>> {
    let stripped_bytes = strip(text.as_bytes());
    let stripped_str = String::from_utf8(stripped_bytes)
        .unwrap_or_else(|_| text.to_string());
    Ok(PyString::new(py, &stripped_str))
}

/// A Python module implemented in Rust.
#[pymodule]
fn euxis_tui_rs(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add_function(wrap_pyfunction!(strip_ansi, m)?)?;
    Ok(())
}
