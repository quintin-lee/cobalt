# Contributing to Cobalt

Thank you for your interest in contributing to Cobalt!

## Development Workflow

1. Fork the repository
2. Create a feature branch (`git checkout -b feat/your-feature`)
3. Make your changes
4. Run tests (`cd build && ctest`)
5. Commit with [gitmoji](https://gitmoji.dev/) format
6. Push and create PR

## Commit Format

```
[type](scope): [emoji] subject
```

### Types
- `feat` ✨ - New feature
- `fix` 🐛 - Bug fix
- `docs` 📝 - Documentation
- `refactor` ♻️ - Code restructuring
- `test` ✅ - Tests
- `chore` 🧹 - Maintenance

## Code Style

- C11 standard
- 4-space indentation
- K&R brace style
- snake_case for functions/variables
- Include guards on all headers

## Testing

All changes must pass existing tests:
```bash
cd build
ctest --output-on-failure
```

## Architecture

Cobalt follows an 8-layer architecture:
- L8: Platform (OS abstraction)
- L7: Memory & Runtime
- L6: Object System
- L5: Interfaces
- L4: Containers
- L3: Algorithms
- L2: Modules
- L1: Applications

See [RFC/COBALT_L1_OBJECT_BASE_v2.md](docs/RFC/COBALT_L1_OBJECT_BASE_v2.md) for details.
