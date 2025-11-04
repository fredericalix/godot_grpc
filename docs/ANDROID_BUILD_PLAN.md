# Plan de Build Android pour godot_grpc

## Vue d'ensemble

Compiler `godot_grpc` pour Android est complexe car il nécessite :
- Android NDK pour la cross-compilation
- Compiler gRPC et protobuf pour 4 architectures Android (arm64-v8a, armeabi-v7a, x86, x86_64)
- Gérer les triplets vcpkg spécifiques pour chaque ABI Android
- Intégration avec le système de build Godot Android

## Défis Techniques

### 1. **Multiples ABIs Android**
Android supporte 4 ABIs principales :
- `arm64-v8a` : ARM 64-bit (moderne, prioritaire)
- `armeabi-v7a` : ARM 32-bit (anciens devices)
- `x86_64` : Intel/AMD 64-bit (émulateurs)
- `x86` : Intel/AMD 32-bit (anciens émulateurs)

**Recommandation** : Commencer avec uniquement `arm64-v8a` et `armeabi-v7a` (couvrent 99%+ des devices physiques)

### 2. **vcpkg avec Android NDK**
vcpkg supporte Android mais nécessite :
- Configuration complexe avec ANDROID_NDK_HOME
- Triplets personnalisés pour chaque ABI
- Chaînage du toolchain Android avec vcpkg

### 3. **Taille des Dépendances**
gRPC + protobuf + abseil compilés pour Android = ~100-150 MB par ABI
- Total pour 2 ABIs : ~200-300 MB
- Impact sur la taille de l'APK final
- Temps de build CI : 30-60 minutes par ABI

### 4. **CMake Cross-Compilation**
Nécessite configuration spéciale :
- `ANDROID_ABI` : L'ABI cible
- `ANDROID_PLATFORM` : API level minimum (21 = Android 5.0)
- `ANDROID_NDK` : Chemin vers NDK
- `CMAKE_TOOLCHAIN_FILE` : Chaînage NDK + vcpkg

## Plan d'Implémentation

### Phase 1 : Configuration Locale (Développement)

#### Prérequis
```bash
# 1. Installer Android NDK
# Option A : Via Android Studio SDK Manager
# Option B : Téléchargement direct
wget https://dl.google.com/android/repository/android-ndk-r26d-darwin.dmg

# 2. Exporter variables d'environnement
export ANDROID_NDK_HOME=/path/to/android-ndk-r26d
export ANDROID_SDK_ROOT=/path/to/android-sdk
```

#### Triplets vcpkg Personnalisés

Créer `vcpkg-triplets/arm64-android.cmake` :
```cmake
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=aarch64-linux-android")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DANDROID_ABI=arm64-v8a
    -DANDROID_PLATFORM=21
)
```

Créer `vcpkg-triplets/arm-android.cmake` :
```cmake
set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=armv7a-linux-androideabi")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DANDROID_ABI=armeabi-v7a
    -DANDROID_PLATFORM=21
)
```

#### Script de Build Local

Créer `scripts/build_android.sh` :
```bash
#!/bin/bash
set -e

# Configuration
ANDROID_ABI=${1:-arm64-v8a}  # arm64-v8a ou armeabi-v7a
BUILD_TYPE=${2:-Release}
ANDROID_API=21

if [ "$ANDROID_ABI" == "arm64-v8a" ]; then
    VCPKG_TRIPLET="arm64-android"
else
    VCPKG_TRIPLET="arm-android"
fi

echo "Building for Android ABI: $ANDROID_ABI"
echo "Build type: $BUILD_TYPE"

# Vérifier que NDK est installé
if [ -z "$ANDROID_NDK_HOME" ]; then
    echo "Error: ANDROID_NDK_HOME not set"
    exit 1
fi

# Clean build directory
rm -rf build-android-$ANDROID_ABI
mkdir -p build-android-$ANDROID_ABI
cd build-android-$ANDROID_ABI

# Configure avec vcpkg + NDK toolchain
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DVCPKG_TARGET_TRIPLET=$VCPKG_TRIPLET \
    -DANDROID_ABI=$ANDROID_ABI \
    -DANDROID_PLATFORM=$ANDROID_API \
    -DANDROID_STL=c++_shared \
    -DUSE_VCPKG=ON

# Build
cmake --build . --config $BUILD_TYPE -j$(nproc)

echo "Build complete: demo/addons/godot_grpc/bin/android/$ANDROID_ABI/"
```

#### Mise à jour CMakeLists.txt

Ajouter support Android :
```cmake
# Platform detection
if(ANDROID)
    set(GODOT_PLATFORM "android")
    set(GODOT_ARCH "${ANDROID_ABI}")
elseif(APPLE)
    # ... existing macOS code
```

Ajouter suffixe Android :
```cmake
# Platform-specific settings
if(ANDROID)
    set_target_properties(${LIBRARY_NAME} PROPERTIES
        SUFFIX ".so"
    )
    # Android nécessite c++_shared
    target_link_libraries(${LIBRARY_NAME} PRIVATE log android)
elseif(APPLE)
    # ... existing macOS code
```

### Phase 2 : Test Local

```bash
# Build pour arm64-v8a
export VCPKG_ROOT=/path/to/vcpkg
export ANDROID_NDK_HOME=/path/to/ndk
./scripts/build_android.sh arm64-v8a Release

# Build pour armeabi-v7a
./scripts/build_android.sh armeabi-v7a Release

# Vérifier les binaires
ls -lh demo/addons/godot_grpc/bin/android/*/
```

### Phase 3 : GitHub Actions CI

Créer `.github/workflows/build-android.yml` :
```yaml
name: Build Android

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]
  workflow_dispatch:

jobs:
  build-android:
    name: Build Android
    runs-on: ubuntu-22.04
    timeout-minutes: 120
    strategy:
      matrix:
        abi: [arm64-v8a, armeabi-v7a]
        build_type: [Release]

    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Set up Java
        uses: actions/setup-java@v4
        with:
          distribution: 'temurin'
          java-version: '17'

      - name: Setup Android NDK
        uses: nttld/setup-ndk@v1
        with:
          ndk-version: r26d
          add-to-path: true

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential \
            cmake \
            ninja-build \
            pkg-config \
            curl \
            zip \
            unzip \
            tar

      - name: Setup vcpkg cache
        uses: actions/cache@v4
        with:
          path: |
            vcpkg/installed
            vcpkg/packages
            vcpkg/buildtrees
          key: vcpkg-android-${{ matrix.abi }}-${{ hashFiles('vcpkg.json') }}
          restore-keys: |
            vcpkg-android-${{ matrix.abi }}-

      - name: Install vcpkg
        run: |
          git clone https://github.com/microsoft/vcpkg.git
          cd vcpkg
          ./bootstrap-vcpkg.sh
          echo "VCPKG_ROOT=$PWD" >> $GITHUB_ENV

      - name: Clone godot-cpp
        run: |
          git clone https://github.com/godotengine/godot-cpp
          cd godot-cpp
          git checkout 4.3

      - name: Create vcpkg triplet
        run: |
          if [ "${{ matrix.abi }}" == "arm64-v8a" ]; then
            cat > $VCPKG_ROOT/triplets/community/arm64-android.cmake << 'EOF'
          set(VCPKG_TARGET_ARCHITECTURE arm64)
          set(VCPKG_CRT_LINKAGE dynamic)
          set(VCPKG_LIBRARY_LINKAGE static)
          set(VCPKG_CMAKE_SYSTEM_NAME Android)
          set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=21)
          EOF
            echo "VCPKG_TRIPLET=arm64-android" >> $GITHUB_ENV
          else
            cat > $VCPKG_ROOT/triplets/community/arm-android.cmake << 'EOF'
          set(VCPKG_TARGET_ARCHITECTURE arm)
          set(VCPKG_CRT_LINKAGE dynamic)
          set(VCPKG_LIBRARY_LINKAGE static)
          set(VCPKG_CMAKE_SYSTEM_NAME Android)
          set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=21)
          EOF
            echo "VCPKG_TRIPLET=arm-android" >> $GITHUB_ENV
          fi

      - name: Configure CMake
        run: |
          mkdir build
          cd build
          cmake .. \
            -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
            -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
            -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
            -DVCPKG_TARGET_TRIPLET=$VCPKG_TRIPLET \
            -DANDROID_ABI=${{ matrix.abi }} \
            -DANDROID_PLATFORM=21 \
            -DANDROID_STL=c++_shared \
            -DUSE_VCPKG=ON

      - name: Build
        run: |
          cd build
          cmake --build . --config ${{ matrix.build_type }} -j$(nproc)

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: android-${{ matrix.abi }}-${{ matrix.build_type }}
          path: demo/addons/godot_grpc/bin/android/${{ matrix.abi }}/*
          retention-days: 7
```

### Phase 4 : Intégration avec build-release.yml

Ajouter le job Android et l'inclure dans `create-release` :
```yaml
needs: [build-macos, build-linux, build-windows, build-android]
```

Copier les artifacts Android dans la release :
```bash
cp -r artifacts/android-arm64-v8a-Release/* release/addons/godot_grpc/bin/android/arm64-v8a/ || true
cp -r artifacts/android-armeabi-v7a-Release/* release/addons/godot_grpc/bin/android/armeabi-v7a/ || true
```

## Problèmes Anticipés et Solutions

### 1. Temps de Build Excessif
**Problème** : gRPC prend 30-60 min à compiler par ABI
**Solutions** :
- Utiliser `actions/cache` pour vcpkg packages
- Limiter à 2 ABIs prioritaires
- Build en parallèle avec matrix strategy

### 2. Taille des Artifacts
**Problème** : Binaires Android sont volumineux
**Solutions** :
- Strip symbols avec `-Wl,--strip-all` en Release
- Utiliser `VCPKG_LIBRARY_LINKAGE=static` pour éviter dépendances
- Compresser artifacts avant upload

### 3. Compatibilité NDK
**Problème** : Différentes versions NDK peuvent causer des incompatibilités
**Solutions** :
- Fixer version NDK (r26d recommandée)
- Tester avec même version NDK en local et CI
- Documenter version NDK requise

### 4. Erreurs de Linkage
**Problème** : `c++_shared` vs `c++_static`
**Solutions** :
- Toujours utiliser `ANDROID_STL=c++_shared` (compatible Godot)
- Vérifier que toutes dépendances utilisent le même STL
- Utiliser `ldd` pour vérifier dépendances

## Estimation de Temps

- **Configuration initiale** : 4-6 heures
  - Créer triplets vcpkg
  - Configurer CMakeLists.txt
  - Tester build local

- **Premier build réussi** : 2-3 heures
  - Débugger erreurs de configuration
  - Ajuster flags de compilation

- **CI/CD setup** : 2-4 heures
  - Configurer GitHub Actions
  - Optimiser caching
  - Tester builds

- **Total estimé** : 8-13 heures

## Ordre d'Exécution Recommandé

1. ✅ **Installer Android NDK** localement
2. ✅ **Créer triplets vcpkg** pour arm64-v8a
3. ✅ **Modifier CMakeLists.txt** pour Android
4. ✅ **Test build local** arm64-v8a
5. ✅ **Créer script build_android.sh**
6. ✅ **Test build local** armeabi-v7a
7. ✅ **Créer workflow GitHub Actions**
8. ✅ **Tester CI build**
9. ✅ **Optimiser caching**
10. ✅ **Intégrer dans build-release.yml**

## Alternatives Plus Simples

Si le build complet s'avère trop complexe :

### Option A : Build Manual avec Documentation
- Ne pas automatiser dans CI
- Fournir documentation complète pour build local
- Distribuer binaires pré-compilés via GitHub Releases

### Option B : Utiliser Docker
- Créer image Docker avec NDK + vcpkg pré-configuré
- Simplifie configuration locale et CI
- Overhead de maintenance d'image Docker

### Option C : Godot Module au lieu de GDExtension
- Compiler directement dans Godot engine
- Plus simple pour Android
- Moins flexible (requiert recompilation Godot)

## Ressources Utiles

- [vcpkg Android Documentation](https://learn.microsoft.com/en-us/vcpkg/users/platforms/android)
- [Android NDK CMake Guide](https://developer.android.com/ndk/guides/cmake)
- [Godot GDExtension Android Export](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_android.html)
- [gRPC Cross-Compilation](https://grpc.io/docs/languages/cpp/quickstart/)

## Décision Finale

**Recommandation** : Commencer par build local uniquement, sans CI

**Raison** :
1. Android build est le plus complexe (4 ABIs × multiples dépendances)
2. Temps de build CI très long (30-60 min par ABI)
3. Usage Android probablement minoritaire comparé à desktop
4. Peut être ajouté plus tard si demande utilisateurs

**Alternative immédiate** : Fournir binaires pré-compilés manuellement pour releases majeures
