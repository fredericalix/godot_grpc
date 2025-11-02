# Contributing to godot_grpc

Thank you for considering contributing to godot_grpc! This document outlines the process and guidelines for contributing.

## Getting Started

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/yourusername/godot_grpc.git
   cd godot_grpc
   ```
3. Set up your development environment (see README.md)

## Development Workflow

1. Create a new branch for your feature/fix:
   ```bash
   git checkout -b feature/my-new-feature
   ```

2. Make your changes

3. Build and test:
   ```bash
   ./scripts/build_vcpkg.sh Debug
   ```

4. Test in Godot using the demo project

5. Commit your changes:
   ```bash
   git add .
   git commit -m "Add feature: description"
   ```

6. Push to your fork:
   ```bash
   git push origin feature/my-new-feature
   ```

7. Create a Pull Request on GitHub

## Code Style

- **C++**: Follow the existing code style
  - Use 4 spaces for indentation
  - Class names: `PascalCase`
  - Methods/functions: `snake_case`
  - Member variables: `snake_case_` (trailing underscore)
  - Use C++17 features where appropriate

- **GDScript**: Follow [Godot's GDScript style guide](https://docs.godotengine.org/en/stable/tutorials/scripting/gdscript/gdscript_styleguide.html)

- **Comments**: Use clear, concise comments. Document public APIs thoroughly.

## Testing

Before submitting a PR:

1. Build on all target platforms (or specify which platforms you tested)
2. Run the demo project to verify basic functionality
3. Test both Debug and Release builds
4. Verify no memory leaks or crashes

## Pull Request Guidelines

- **Title**: Clear, descriptive title
- **Description**: Explain what changes you made and why
- **Platform Testing**: List which platforms you tested on
- **Breaking Changes**: Clearly mark any breaking API changes
- **Documentation**: Update README.md if adding new features

## Reporting Issues

When reporting issues, include:

- Godot version
- Operating system and version
- Steps to reproduce
- Expected behavior
- Actual behavior
- Error messages or logs
- Minimal reproducible example if possible

## Feature Requests

We welcome feature requests! Please:

1. Check if the feature already exists or is planned
2. Clearly describe the use case
3. Explain how it fits with the project's goals
4. Be open to discussion about implementation

## License

By contributing, you agree that your contributions will be licensed under the BSD 3-Clause License.

## Questions?

Feel free to open an issue for questions or discussion.
