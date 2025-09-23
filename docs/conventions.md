# Conventions and programming guidelines
There are obviously some exceptions, and **some** may be temporarily omitted while developing.

### Includes
Usage
- Angle brackets <> when including from libraries.
- Quotations "" when including internal files.
- Use relative paths when it makes sense for files to be in the same directory (or child directories). If you need to include a file from a parent folder, it is almost always in a different "module" of the project and should be included as an "absolute" path (relative to src). `"util/fastMath.h"` instead of `"../../util/fastMath.h"`.

Include Order
- Include any external libraries first
- If the file is a .cpp file, include the corresponding .h before any other local .h files
- Add line breaks between the different kinds of includes if you want

QT Includes
- Do not use QT includes ending with .h, use the ones specified by classes in the [Qt documentation](https://doc.qt.io/qt-6/classes.html)

### Header Guards

Use **#ifndef**
The preprocessor define should be the name of the file with an underscore.
`blockContainer.h`:
```
#ifndef blockContainer_h
#define blockContainer_h

...

#endif \* blockContainer_h *\
```

### Naming
**camelCase** for file names
".h" extension for headers and ".cpp" extension for source files

- variable and function names should be **camelCase**
- const variable and enum state names should be **MACRO_CASE**
- type names should be **PascalCase**

### Using Raw and Smart Pointers
Dont use the `new` keyword except when creating windows for qt to own.
You should use smart pointers for ownership. Sometimes you may find custom smart pointers in the codebase.
You can pass raw pointers around but objects reciving raw pointers should not be responsible for deallocating the memory.

### Enums
Please preface enum states in use with the enum type and the scope resolution operator.
Use `BlockType::AND` instead of `AND`

### Class Declaration Order
Order should be:
- friends
- public
- protected functions
- protected vars
- private functions
- private vars

### Friend Classes
You can use friend classes to simplify APIs. Please indicate which private functions are meant to be used by which friend classes.

### Assertions
For now, you can use assertions to confirm state that is **essential** for the program to run.

### Returning Nothing
When functions can return nothing, use `std::optional`

### Integer
You should use regular c++ types when the size isn't important. You should use cstdint types when you need an exact size.

### Tabs vs. Spaces
Use tabs please

### Small TODO comments
```cpp
// TODO: changes to be made
```

### Curly Brackets
You should open them on the same line. Put a space before them.
```cpp
// do
if () {
}
```
```cpp
// dont
if ()
{
}
```

### Constructor/initializer lists

Put anything that needs to be constructed in the initializer lists.
Default constructors should not be called.
Primitive and enums should have default values delared in the header.
```
class Foo {
public:
	Foo(int num) : bar(num) {}
private:
	std::string name;
	int bar;
	int count = 0;
}
```
