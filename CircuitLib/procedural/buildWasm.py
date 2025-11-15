import pathlib
import json
import hashlib
import os

import colorama
colorama.init()

def build(source: pathlib.Path, target: pathlib.Path):
    assert (source.suffix in [".zig", ".rs", ".c", ".cpp", ".cc"])
    assert (target.suffix == ".wasm")
    os.chdir(source.parent)
    res = 0
    if source.suffix == ".zig":
        res = os.system(f"zig build-exe {source} -target wasm32-freestanding -fno-entry -rdynamic -O ReleaseSmall -femit-bin={target}")
    elif source.suffix == ".rs":
        return False # Rust builds are currently disabled
        res = os.system(f"rustc {source} -target wasm32-unknown-unknown -O -o {target}")
    elif source.suffix in [".cc", ".c", ".cpp"]:
        res = os.system(f"emcc {source} -Os -flto -s STANDALONE_WASM -s EXPORTED_FUNCTIONS=\"['_generateCircuit', '_getUUID', '_getName', '_getDefaultParameters']\" --no-entry -o {target}")
    return res == 0

BuildCache = dict[str, str]

def load_build_cache(path: pathlib.Path) -> BuildCache:
    if not path.exists():
        return {}
    with open(path, "r") as f:
        return json.load(f)

def get_relative_path(file_path: pathlib.Path, parent: pathlib.Path) -> str:
    try:
        return file_path.resolve().relative_to(parent).as_posix()
    except ValueError:
        return file_path.resolve().as_posix()

def normalize_cache(build_cache: BuildCache, script_dir: pathlib.Path) -> tuple[BuildCache, bool]:
    normalized_cache: BuildCache = {}
    cache_normalized = False
    for path_str, hash_val in build_cache.items():
        normalized_key = get_relative_path(pathlib.Path(path_str), script_dir)
        if normalized_key != path_str:
            cache_normalized = True
        normalized_cache[normalized_key] = hash_val
    return normalized_cache, cache_normalized

def get_all_source_files_from_folder(folder_path: pathlib.Path) -> list[pathlib.Path]:
    for file_group in folder_path.walk():
        for filename in file_group[2]:
            file = file_group[0] / pathlib.Path(filename)
            yield file

def get_all_filtered_source_files_from_folder(folder_path: pathlib.Path) -> list[pathlib.Path]:
    for file in get_all_source_files_from_folder(folder_path):
        if file.suffix not in [".zig", ".rs", ".c", ".cpp", ".cc"]:
            continue
        if file.stem == "cm":
            continue
        yield file

def get_all_source_files(folder_paths: list[pathlib.Path], script_dir: pathlib.Path) -> list[pathlib.Path]:
    file_paths: list[pathlib.Path] = []
    for folder_path in folder_paths:
        for file in get_all_filtered_source_files_from_folder(folder_path):
            file_paths.append(file)
    return file_paths

def calculate_file_hash(file_path: pathlib.Path) -> str:
    hasher = hashlib.new("md5")
    with file_path.open("r", encoding="utf-8", newline=None) as f:
        while True:
            data = f.read(8192)
            if not data:
                break
            hasher.update(data.encode("utf-8"))
    return hasher.hexdigest()

def build_file_considering_cache(file: pathlib.Path, build_cache: BuildCache, target_directory: pathlib.Path, script_dir: pathlib.Path):
    file_hash = calculate_file_hash(file)
    file_relative = get_relative_path(file, script_dir)
    target_file = target_directory / pathlib.Path(file.stem + '.wasm')
    target_file_relative = get_relative_path(target_file, script_dir)
    if file_relative in build_cache:
        entry = build_cache[file_relative]
        if entry == file_hash and pathlib.Path(script_dir / target_file_relative).exists():
            return False, False
    print(colorama.Fore.GREEN + "Building" + colorama.Style.RESET_ALL, file)
    success = build(file, target_file)
    if not success:
        print(colorama.Fore.RED + "Build failed for" + colorama.Style.RESET_ALL, file)
        return False, True
    build_cache[file_relative] = file_hash
    return True, False

def build_files(file_paths: list[pathlib.Path], build_cache: BuildCache, target_directory: pathlib.Path, script_dir: pathlib.Path) -> bool:
    build_cache_updated = False
    error_occured = False
    for file in file_paths:
        built, error = build_file_considering_cache(file, build_cache, target_directory, script_dir)
        if error:
            error_occured = True
        if built:
            build_cache_updated = True
    return build_cache_updated, error_occured

def main():
    script_dir = pathlib.Path(__file__).parent.resolve()

    build_cache_path = script_dir/"build_cache.json"
    build_cache: BuildCache = load_build_cache(build_cache_path)

    normalized_cache, cache_normalized = normalize_cache(build_cache, script_dir)
    build_cache = normalized_cache

    folders = [
        "cpp",
        "zig",
        # "rust",
    ]

    folder_paths = [(script_dir/folder).resolve() for folder in folders]
    target_directory = (script_dir/"wasm").resolve()

    file_paths = get_all_source_files(folder_paths, script_dir)
    build_cache_updated, error_occured = build_files(file_paths, build_cache, target_directory, script_dir)
    if build_cache_updated or cache_normalized:
        with open(build_cache_path, "w") as f:
            json.dump(build_cache, f, indent=4)

    if not build_cache_updated and not error_occured:
        print(colorama.Fore.GREEN + "All files are up to date :)" + colorama.Style.RESET_ALL)

if __name__ == "__main__":
    main()