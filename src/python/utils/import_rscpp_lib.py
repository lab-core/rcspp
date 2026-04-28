import os
import sys

def import_rscpp_lib():
    os_name = sys.platform

    if os_name == 'win32':
        relative_path = "../../out/build/x64-release/lib/"
        absolute_path = os.path.abspath(relative_path)
        os.add_dll_directory(absolute_path)
        sys.path.append(absolute_path)

        rcspp_path = relative_path + "/rcspp/"
        sys.path.append(rcspp_path)
    elif os_name == 'darwin':
        relative_path = "../../out/build/x64-release/lib/"
        absolute_path = os.path.abspath(relative_path)
        os.add_dll_directory(absolute_path)
        sys.path.append(absolute_path)

        rcspp_path = relative_path + "/rcspp/"
        sys.path.append(rcspp_path)
    elif os_name == 'linux':
        relative_path = "../../build/lib/"
        absolute_path = os.path.abspath(relative_path)
        if "LD_LIBRARY_PATH" in os.environ:
            os.environ["LD_LIBRARY_PATH"] = f"{absolute_path}:{os.environ['LD_LIBRARY_PATH']}"
        else:
            os.environ["LD_LIBRARY_PATH"] = absolute_path
        sys.path.append(absolute_path)

        rcspp_path = relative_path
        sys.path.append(rcspp_path)
    else:
        print(f"OS inconnu: {os_name}")