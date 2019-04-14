# Git repository rules
## Commiting
* Use descriptive commit messages;
* Avoid commits with messages like `WIP`. Squash or ammend these commits.
## Branching
* Create new branch with prefix `feature/` based on `dev` for every new feature;
* Create new branch with prefix `bugfix/` based on `dev` for every bug fix;
# Coding rules
## Naming convention
* Avoid abbreviation except commonly used ones like `JSON` or `TCP`;
* Use `snake_case` for classes, enums, namespaces, variables and constants;
* For private member variables use `_` postfix;
* For macro names use `UPPER_CASE_WITH_UNDERSCORES`;
* For template parameters use `CamelCase`.
## Other rules
* Always use `{}` for blocks like `for` of `if`;
* Always use `nullptr` instead of `NULL`;
* Prefer `//` comments to `/**/`;
* Avoid use 'using namespace' in a header file;
* Prefer `#pragma once` as include guard;
* Line length should be less than 80. (120 in some cases);
* Includes order:
* * Corresponding header file;
* * C includes;
* * C++ includes;
* * Boost lib includes;
* * Other lib includes;
* * Local file includes.
* Namespace closing bracket should have comment `// namespace name_of_namespace`;
* Always use space for indentation;
* Use const where possible;
* Always use `override` keyword;
* Avoid globals;
* Prefer `unique_ptr` to `shared_ptr`;
* Prefer `++i` to `i++`;