// Minimal JavaScript fixture used by libs/parse tests. Covers ES2024
// patterns: arrow functions, optional chaining, nullish coalescing,
// async/await, destructuring, classes, template literals.

class Greeting {
    constructor(subject, reps = 1) {
        this.subject = subject;
        this.reps = reps;
    }

    render() {
        return `Hello, ${this.subject}!`;
    }
}

const add = (a, b) => a + b;

async function main() {
    const g = new Greeting('world', add(40, 2));
    const lines = Array.from({ length: g.reps }, (_, i) => `${i}: ${g.render()}`);
    for await (const line of lines) {
        console.log(line);
    }
    const safe = (input) => input?.value ?? 'fallback';
    return safe(null);
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});
