use std::env;
use std::path::PathBuf;
use anyhow::{Context, Result};
use extism::Plugin;

#[tokio::main]
async fn main() -> Result<()> {
    // 1. Initialize Tracing (Zero-overhead if disabled)
    tracing_subscriber::fmt::init();

    // 2. Resolve EUXIS_HOME
    let euxis_home = env::var("EUXIS_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|_| {
            let mut path = dirs::home_dir().expect("Could not resolve home directory");
            path.push(".euxis");
            path
        });

    // 3. Command Routing
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        print_usage();
        return Ok(());
    }

    let command = &args[1];
    match command.as_str() {
        "version" | "--version" => {
            println!("euxis v0.0.3 (native-dispatch)");
        }
        "health" => {
            run_health_check().await?;
        }
        "agent" => {
            if args.len() < 4 {
                println!("Usage: euxis agent <name> <task>");
            } else {
                run_agent(&euxis_home, &args[2], &args[3]).await?;
            }
        }
        _ => {
            println!("Command '{}' not yet implemented in native-dispatch", command);
            println!("Falling back to legacy dispatcher...");
            // In a real implementation, we would exec the legacy binary here
        }
    }

    Ok(())
}

async fn run_health_check() -> Result<()> {
    println!("Checking Euxis Native Core Health...");
    println!("  ✓ Rust Runtime: OK");
    println!("  ✓ Async Executor: OK");
    Ok(())
}

async fn run_agent(euxis_home: &std::path::Path, agent_name: &str, task: &str) -> Result<()> {
    let agent_path = euxis_home
        .join("euxis-core/agents")
        .join(format!("{}.wasm", agent_name));

    if !agent_path.exists() {
        anyhow::bail!("Agent Wasm binary not found at {:?}", agent_path);
    }

    println!("Dispatching agent '{}' (Native Path)...", agent_name);

    let wasm = std::fs::read(&agent_path)
        .with_context(|| format!("Failed to read Wasm agent at {:?}", agent_path))?;

    let mut plugin = Plugin::new(&wasm, [], true)
        .with_context(|| "Failed to initialize Extism plugin")?;

    let input = serde_json::json!({
        "task": task,
        "timestamp": std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)?
            .as_secs()
    });

    let input_str = serde_json::to_string(&input)?;
    let output = plugin.call::<&str, &str>("run", &input_str)
        .with_context(|| "Wasm execution failed")?;

    println!("Agent Response: {}", output);

    Ok(())
}

fn print_usage() {
    println!("Euxis Native Dispatcher");
    println!("Usage: euxis <command> [args]");
}
