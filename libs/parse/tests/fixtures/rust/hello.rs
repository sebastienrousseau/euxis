// Minimal Rust fixture used by libs/parse tests. Exercises function
// definitions, struct types, impl blocks, generic constraints, and
// the `Result` enum — the grammar paths most rust code we'll scan
// goes through.

use std::fmt::Display;

#[derive(Debug, Clone)]
struct Greeting<T: Display> {
    subject: T,
    reps: u32,
}

impl<T: Display> Greeting<T> {
    fn new(subject: T) -> Self {
        Self { subject, reps: 1 }
    }

    fn render(&self) -> String {
        format!("Hello, {}!", self.subject)
    }
}

fn add(a: i32, b: i32) -> i32 {
    a + b
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let sum = add(40, 2);
    let g = Greeting::new("world");
    println!("{} ({})", g.render(), sum);
    Ok(())
}
