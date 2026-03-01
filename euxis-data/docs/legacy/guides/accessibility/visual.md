# Visual Accessibility Themes

Euxis implements the Terminal User Interface (TUI) prioritizing readability, minimal screen tearing, and strict semantic layout models natively. 

## Enforcing High Contrast

If you suffer from astigmatism or simply operate effectively in glare-heavy terminal environments natively, configure the TUI purely for structural contrast over stylistic vibrancy natively.

Bind the `EUXIS_THEME` environment flag directly internally inside your `.bashrc` profile.

```bash
export EUXIS_THEME="monochrome"
```

This strips background shading intrinsically, rendering purely ANSI 16-color font variations entirely against absolute `#000000` arrays.

## Disabling Generative Animations

The TUI generates fluid transition graphs natively when spinning up specific Wasm agents. For environments utilizing remote SSH shells natively or for users prone strictly to motion-induced ocular fatigue natively, globally disable structural rendering primitives entirely.

Add the following to your global `config.toml`:

```toml
[tui]
animations = false
refresh_rate_ms = 500 # Slow down the telemetry pump entirely natively.
```

## Scaling the Font Typography

Euxis strictly inherits your physical terminal emulator font architectures identically. 

We highly recommend installing and enforcing the `JetBrains Mono` or `Fira Code` Nerd Fonts natively within Alacritty or Kitty explicitly to guarantee appropriate icon and ligature interpretation across the entire Euxis array natively.
