## Summary

<!-- Describe the change in 1-3 bullet points -->

## Test Plan

<!-- How was this tested? -->

- [ ] `make cpp-test` — All tests pass
- [ ] `euxis health` — Fleet integrity check passes
- [ ] `euxis certify-readiness .` — Certification gates pass
- [ ] Manual verification

## Quality Checklist

- [ ] Code compiles with `-Werror` (no warnings)
- [ ] Documentation updated (README.md, user-guide.md, cli-reference.md)
- [ ] No secrets or credentials committed
- [ ] Commits are GPG-signed
- [ ] C++23 standards followed (no raw pointers, RAII, const-correct)

---

> Designed by Sebastien Rousseau — https://sebastienrousseau.com
> Engineered with Euxis — Enterprise Unified Execution Intelligence System — https://euxis.co
