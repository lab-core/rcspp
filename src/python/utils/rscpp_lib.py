import os
import sys
from utils.definitions import RSCPP_DIR


os_name = sys.platform
absolute_path = RSCPP_DIR

if os_name == 'win32':
    os.add_dll_directory(absolute_path)
    sys.path.append(absolute_path)

    rcspp_path = absolute_path + "/rcspp/"
elif os_name == 'darwin':
    os.add_dll_directory(absolute_path)

    rcspp_path = absolute_path + "/rcspp/"
    sys.path.append(rcspp_path)
elif os_name == 'linux':
    if "LD_LIBRARY_PATH" in os.environ:
        os.environ["LD_LIBRARY_PATH"] = f"{absolute_path}:{os.environ['LD_LIBRARY_PATH']}"
    else:
        os.environ["LD_LIBRARY_PATH"] = absolute_path
else:
    print(f"OS inconnu: {os_name}")

sys.path.append(absolute_path)