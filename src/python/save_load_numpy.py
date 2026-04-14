import numpy as np
import json
import pickle

def sauvegarder_variables_npz(matrices, dicts, filepath='checkpoint.npz'):
    """
    Sauvegarde matrices NumPy et dictionnaires dans un fichier .npz.
    
    Args:
        matrices (dict): Dictionnaire des matrices NumPy
        dicts (dict): Dictionnaire des dictionnaires Python
        filepath (str): Chemin du fichier
    
    Exemple:
        sauvegarder_variables_npz(
            {'A': np.array(...), 'B': np.array(...)},
            {'params': {...}, 'config': {...}}
        )
    """
    # Sérialiser les dicts en JSON pour éviter les problèmes d'encodage
    matrices_to_save = matrices.copy()
    
    # Ajouter les dicts sérialisés en JSON
    for key, d in dicts.items():
        try:
            matrices_to_save[f'__dict__{key}'] = np.bytes_(json.dumps(d))
        except (TypeError, ValueError):
            # Si JSON échoue, utiliser pickle
            matrices_to_save[f'__dict__{key}'] = np.bytes_(pickle.dumps(d))
    
    np.savez(filepath, **matrices_to_save)
    print(f"✓ Données sauvegardées dans {filepath}")

def charger_variables_npz(filepath='checkpoint.npz'):
    """
    Charge les matrices et dictionnaires depuis un fichier .npz.
    
    Returns:
        tuple: (dictionnaire_matrices, dictionnaire_dicts)
    """
    data = np.load(filepath, allow_pickle=True)
    
    matrices = {}
    dicts = {}
    
    for key in data.files:
        if key.startswith('__dict__'):
            dict_key = key.replace('__dict__', '')
            serialized = bytes(data[key])
            try:
                # Essayer JSON d'abord
                dicts[dict_key] = json.loads(serialized.decode('utf-8'))
            except:
                # Fallback sur pickle
                dicts[dict_key] = pickle.loads(serialized)
        else:
            matrices[key] = data[key]
    
    print(f"✓ Données chargées depuis {filepath}")
    return matrices, dicts