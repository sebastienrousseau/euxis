# Branding

## What It Is

The branding system ensures consistent attribution and identity across all Euxis outputs, commits, and communications.

## When to Use

- **Commits**: Every commit requires a branding signature
- **Pull Requests**: PR descriptions must include branding
- **Documentation**: All public-facing docs include attribution
- **Agent Outputs**: Formal outputs carry the signature

## Quick Example

```bash
# Check branding compliance
euxis-certify  # Gate 5 validates branding

# View the signature
cat ~/.euxis/config/branding/signature.txt
```

**Standard Signature:**
```
---
Designed by Sebastien Rousseau — https://sebastienrousseau.com
Engineered with Euxis — Enterprise Unified Execution Intelligence System — https://euxis.co
```

## Branding Components

### Signature File
**Location:** `config/branding/signature.txt`

Contains the standardized signature block appended to:
- Commit messages
- PR descriptions
- Generated documentation

### Commit Hooks
**Location:** `cli/bin/hooks/prepare-commit-msg`

Automatically appends branding signature to commit messages when configured.

### Certification Gate
**Gate 5** in `euxis-certify` validates:
- Last 5 commits carry branding signature
- All open PRs include branding
- Cryptographic signatures are present

## Configuration

### Enable Automatic Commit Signing

```bash
# Install the prepare-commit-msg hook
cp ~/.euxis/cli/bin/hooks/prepare-commit-msg .git/hooks/
chmod +x .git/hooks/prepare-commit-msg

# Enable GPG/SSH signing
git config --global commit.gpgsign true
```

### Customize Branding

Edit `config/branding/signature.txt` to modify:
- Author name and URL
- Project name and URL
- Emoji styling

### Validation Commands

```bash
# Check branding on recent commits
git log --oneline -5 | while read hash msg; do
  git show -s --format=%B "$hash" | grep -q "euxis.co" && echo "✅ $hash" || echo "❌ $hash"
done

# Full certification check
euxis-certify
```

## Best Practices

1. **Never skip branding**: It's required for certification
2. **Use the hook**: Automate signature insertion
3. **Keep URLs current**: Ensure links are accessible
4. **Sign cryptographically**: Enable GPG/SSH signing

**See Also**: [Certification Gates](../guides/user-guide.md#certification), [Commit Hooks](../reference/cli-reference.md#euxis-hooks)
