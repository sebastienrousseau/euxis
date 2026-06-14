// Minimal Java fixture used by libs/parse tests. Covers package,
// imports, sealed types, records, pattern matching for instanceof,
// switch expressions, generics, and lambdas — the Java 17+ paths
// most production code we'll scan exercises.

package euxis.sample;

import java.util.List;
import java.util.Optional;

public sealed interface Outcome<T> permits Outcome.Ok, Outcome.Err {
    record Ok<T>(T value) implements Outcome<T> {}
    record Err<T>(String error) implements Outcome<T> {}
}

public final class Hello {

    public record Greeting(String subject, int reps) {
        public String render() {
            return "Hello, " + subject + "!";
        }
    }

    public static int add(int a, int b) {
        return a + b;
    }

    public static Outcome<Greeting> make(String subject) {
        if (subject == null || subject.isBlank()) {
            return new Outcome.Err<>("subject required");
        }
        return new Outcome.Ok<>(new Greeting(subject, 1));
    }

    public static void main(String[] args) {
        var outcome = make("world");
        var summary = switch (outcome) {
            case Outcome.Ok<Greeting> ok -> ok.value().render();
            case Outcome.Err<Greeting> err -> "error: " + err.error();
        };
        var count = add(40, 2);
        List.of(1, 2, 3).forEach(i -> System.out.println(i + ": " + summary + " (" + count + ")"));
        Optional.of(summary).ifPresent(System.out::println);
    }
}
