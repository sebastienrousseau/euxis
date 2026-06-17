# AIO Mandate — How to Write Docs in This Repo

This document defines the writing convention used across Euxis documentation so the prose is easy for both human readers and AI retrieval systems to extract, index, and summarise.

## What "AIO" means here

AIO stands for AI-Optimised. The mandate is a small set of writing rules that make a paragraph self-contained enough that an LLM-driven search engine can return it as an answer without dragging in the surrounding context. Humans read better too — the same rules amount to "say the answer first, then the why, then the details."

## The three rules

### Direct-answer leads

The first sentence after every heading must state the answer or define the concept. No throat-clearing, no scene-setting, no "this section covers." A reader (or a retrieval model) who only sees the heading plus the first sentence should already know what the section is about.

Example. Under a heading "`AgentLoopHarness`", the first sentence is `AgentLoopHarness wraps an iteration budget, a context-compaction engine, and a conversation vector so a CLI command can drive one agent turn without hand-rolling the wiring.` Not `In this section, we discuss the agent loop harness.`

### Conversational tone

Write the way you would explain code to a peer over coffee. Active voice, concrete nouns, no marketing prose. Avoid `leverage`, `unlock`, `seamless`, `holistic`, `next-generation`. Prefer `use`, `enable`, `runs cleanly`, `covers`, `the latest`.

This isn't about being chatty — it's about lowering the cognitive cost of the first read. Conversational prose forces you to commit to claims; marketing prose lets you hide behind adjectives.

### Atomic paragraphs

Each paragraph should be self-contained. A reader landing in the middle of a long doc should understand a paragraph without scrolling up. This means: spell out the subject of each paragraph instead of relying on pronouns that point at the previous heading, and keep each paragraph focused on one claim.

When a thought is genuinely multi-step, break it into two paragraphs with their own leads rather than one paragraph with three sub-points threaded by transitions. Retrieval systems index paragraphs; threaded narration doesn't survive extraction.

## What this is not

The AIO mandate does not require Markdown headings every few sentences, FAQ-style Q&A formatting, or schema markup. Those are SEO tactics from a different decade. The mandate is purely about how the prose reads — heading structure stays driven by the document's logic, not by retrieval-bait.

## How to apply this to an existing doc

Open the doc, scan for the first sentence after each heading, and ask: does it directly answer the heading's question? If not, rewrite it. Then read each paragraph and ask: would this make sense to a reader who only saw this paragraph? If not, restructure.

Most docs don't need a full rewrite. The biggest wins come from rewriting opening paragraphs and the first sentence under each subsection heading; the rest usually follows the same voice once the leads are set.

## When to skip the mandate

Reference tables (function signatures, command-line flags, schema field definitions) are exempt — the table itself is the answer. Code blocks are exempt. ADRs and design documents that follow a fixed template (Status / Context / Decision / Consequences) are exempt; the template enforces the structure for them.

For everything else — guides, architecture overviews, README files, API conceptual docs, example READMEs — the mandate applies.
