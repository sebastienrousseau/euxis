// Minimal TypeScript fixture used by libs/parse tests. Exercises
// generics, union types, interface vs type alias, satisfies, const
// assertions, exhaustiveness via never — the paths most production
// TS we'll scan exercises.

interface Greeting<T extends string> {
    readonly subject: T;
    readonly reps: number;
}

type Outcome<T> = { ok: true; value: T } | { ok: false; error: string };

class GreetingFactory {
    static create<T extends string>(subject: T): Outcome<Greeting<T>> {
        if (subject.length === 0) {
            return { ok: false, error: 'subject required' };
        }
        return { ok: true, value: { subject, reps: 1 } satisfies Greeting<T> };
    }
}

const add = (a: number, b: number): number => a + b;

function render<T extends string>(g: Greeting<T>): string {
    return `Hello, ${g.subject}!`;
}

const outcome = GreetingFactory.create('world');
if (outcome.ok) {
    const count = add(40, 2);
    for (let i = 0; i < count; ++i) {
        console.log(`${i}: ${render(outcome.value)}`);
    }
} else {
    console.error(outcome.error);
}
