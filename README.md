# PixelFrog — operator quick start

Command-line C++17 demo that loads JPEG/PNG, applies filters (greyscale, Gaussian blur, Sobel edges, brighten/darken), and writes an output image. The project is built for **JFrog SaaS + Conan + GitHub Actions** walkthroughs (Artifactory, Xray, Build Info, Release Bundles).

**Time budget:** about 45–60 minutes for Artifactory + secrets + first green CI run, assuming a clean JFrog trial tenant.

**License:** [GNU General Public License v3.0](LICENSE) (GPL-3.0). Copyleft is intentional so Xray licence rules (e.g. watches on `GPL-3.0`) have something to match in **this** repository—not only in third-party dependencies.

---

## Prerequisites (local macOS)

- Xcode CLT or another C++17 compiler (Apple Clang 13+ or GCC 9+).
- **CMake** 3.22+
- **Python** 3.11+ (for Conan 2.x)
- **Conan** 2.x: `pip install "conan~=2.0"`
- **JFrog CLI** (optional locally): [Install JFrog CLI](https://jfrog.com/help/r/jfrog-cli/installing-the-jfrog-cli)
- A **JFrog SaaS** trial (e.g. `https://<your-subdomain>.jfrog.io`)

ConanCenter uses `stb/cci.20230920` (the spec referenced `cci.20230908`, which is not published on ConanCenter).

---

## 1. Artifactory repository layout (Section 6)

In the JFrog Platform UI (logged in as admin or equivalent):

1. **Administration** → **Repositories** → **Add repository** (or **Repositories** under **Artifacts**, depending on UI version).

### Remote: `conan-remote`

- Type: **Remote**
- Package Type: **Conan**
- URL: `https://center.conan.io` or current ConanCenter remote URL from [ConanCenter docs](https://conan.io/center).
- Repository Key: `conan-remote`

### Local: `conan-local`

- Type: **Local**
- Package Type: **Conan**
- Repository Key: `conan-local`

### Virtual: `conan-virtual`

- Type: **Virtual**
- Package Type: **Conan**
- Include `conan-local` **first**, then `conan-remote`.
- Repository Key: `conan-virtual`  
  This is the **single URL** developers and CI use:  
  `https://<subdomain>.jfrog.io/artifactory/api/conan/conan-virtual`  
  (replace with your host and key if you used different names).

### Local generic: `pixelfrog-generic-local`

- Type: **Local**
- Package Type: **Generic**
- Repository Key: `pixelfrog-generic-local`

**Screenshots:** capture the virtual repo “Included Repositories” list and the generic repo empty state for your demo deck.

---

## 2. Xray policy and watch (Section 7)

### Policy `pixelfrog-security-policy`

1. **Security** → **Policies** → **Create Policy**.
2. Rules (adjust to match your Xray license features):
   - Block or fail builds on **CVSS ≥ 7.0** (High/Critical).
   - Optionally: flag CVEs with **available fixed version**.
   - Licence rule: notify or block on **GPL-3.0** (or broader copyleft, per your demo).

### Watch `pixelfrog-watch`

1. **Security** → **Watches** → **Create Watch**.
2. Resources:
   - Repository: `conan-virtual` (all Conan packages resolved through the virtual).
   - Repository: `pixelfrog-generic-local` (compiled generic binary).
3. Assign policy: `pixelfrog-security-policy`.

This highlights **dependencies + released binary** in one watch.

---

## 3. GitHub Actions variables and OIDC (Section 5.2)

The workflow uses **OpenID Connect** with the [JFrog `setup-jfrog-cli` action](https://github.com/jfrog/setup-jfrog-cli) so you do **not** need a long-lived `JF_ACCESS_TOKEN` secret for CI.

### 3.1 Configure trust in JFrog

1. In the JFrog Platform, create an **OIDC integration** for GitHub Actions and an **identity mapping** that matches your repository (and branch/workflow if you lock it down). The **Provider name** you set in JFrog must match `JF_OIDC_PROVIDER` below.
2. See [GitHub Docs: OpenID Connect in JFrog](https://docs.github.com/en/actions/security-for-github-actions/security-hardening-your-deployments/configuring-openid-connect-in-jfrog) and [JFrog’s GitHub OIDC overview](https://jfrog.com/blog/secure-access-development-jfrog-github-oidc/).

### 3.2 Repository variables

In GitHub: **Settings** → **Secrets and variables** → **Actions** → **Variables** → **New repository variable**:

| Variable             | Purpose |
|----------------------|--------|
| `JF_URL`             | Platform URL, e.g. `https://acme.jfrog.io` (no trailing slash). |
| `JF_OIDC_PROVIDER`   | **Provider name** from the JFrog OIDC integration (passed to `oidc-provider-name`). |
| `JF_OIDC_AUDIENCE`   | OIDC **audience** for the GitHub ID token (must match what JFrog expects; often customized per integration). |
| `JF_CONAN_VIRTUAL_REPO` | **Optional.** Artifactory **Conan virtual** repository key used for `conan remote` and as the **first** repo passed to **`conan art:build-info create`**. Defaults to `mwpf-conan-virtual-dev` in the workflow. |
| `JF_CONAN_ART_LOCAL_REPO` | **Optional.** Artifactory **Conan local** repository key passed to **`conan art:build-info create`** (from [conan-io/conan-extensions](https://github.com/conan-io/conan-extensions) `art` commands). Enables Xray-friendly Conan module lists similar to [conan-sbom-generation-demo](https://github.com/sureshvenkatesan1/conan-sbom-generation-demo) and [ps-jfrog/conan-hello-world](https://github.com/ps-jfrog/conan-hello-world/blob/main/.github/workflows/jf-cli.yml). If unset, CI falls back to **`jf conan install --build-name`**. See [Conan 2 — JFrog / Build Info](https://docs.conan.io/2/integrations/jfrog.html). |
| `JF_CONAN_ART_REPO_EXTRA` | **Optional.** Space-separated extra Conan repo keys appended **after** virtual + local (deduplicated). Use if you need another repo (e.g. a dedicated **remote** cache key) for `conanmanifest.txt` lookup. |

The workflow requests `permissions: id-token: write` and passes `JF_URL` plus the two OIDC inputs to `jfrog/setup-jfrog-cli@v4`. The action exchanges the GitHub OIDC token for a JFrog access token, configures the CLI, and exposes **`oidc-user`** / **`oidc-token`** step outputs for `conan remote login`.

### 3.3 Conan remote URL

The workflow sets the Conan API URL to:

`${JF_URL}/artifactory/api/conan/<virtual-key>`

where **`<virtual-key>`** is **`JF_CONAN_VIRTUAL_REPO`**, or `mwpf-conan-virtual-dev` if that variable is unset (see top-level `env` in [`.github/workflows/build.yml`](.github/workflows/build.yml)).

### 3.4 Optional: token-based auth instead of OIDC

To use a static access token, replace the OIDC `with:` block on `setup-jfrog-cli` with `env: JF_URL` + `JF_ACCESS_TOKEN` from a secret and remove reliance on `steps.jfrog.outputs['oidc-token']` for Conan (use the same token in `conan remote login`). The upstream action documents this path in its README.

---

## 4. Local build

```bash
cd pixelfrog
conan profile detect --force   # first machine only
conan install . --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release
```

Binaries land under `build/Release/`.

**Against Artifactory** (after you created `conan-virtual`):

```bash
conan remote add jfrog-conan "https://<subdomain>.jfrog.io/artifactory/api/conan/conan-virtual" --force
conan remote login jfrog-conan <user> -p <token-or-password>
conan install . --build=missing -s build_type=Release -r jfrog-conan
cmake --preset conan-release
cmake --build --preset conan-release
```

---

## 5. Local run examples

```bash
./build/Release/pixelfrog -i test_images/sample.jpg -o result.jpg -f greyscale
./build/Release/pixelfrog -i test_images/sample.jpg -o result.jpg -f blur -f edges -v
./build/Release/pixelfrog -i test_images/sample.png -o result.png -f brighten -c config.json
```

---

## 6. DEMO_VULN_MODE (optional — demos only)

**Do not use in production or shared tenants.** Goal: pull an **older spdlog** so Xray may show issues that newer trees no longer expose.

1. Install with the Conan option:

   ```bash
   conan install . --build=missing -s build_type=Release -o "&:demo_vuln_mode=True"
   ```

2. Reconfigure CMake (toolchain sets `DEMO_VULN_MODE` for compile defs):

   ```bash
   cmake --preset conan-release
   cmake --build --preset conan-release
   ```

The recipe currently pins **`spdlog/1.9.2`** in this mode (ConanCenter no longer serves `1.8.0`). Adjust [`conanfile.py`](conanfile.py) if your Artifactory mirrors a different legacy version.

---

## 7. CI pipeline (`.github/workflows/build.yml`)

Seven jobs, matching the spec: **setup**, **resolve-dependencies**, **build**, **test**, **security-scan**, **package-binary**, **release-bundle** (main pushes only).

- **Release bundles** require Distribution / Release Lifecycle to be enabled; steps echo a hint if promotion is not configured.
- **SARIF upload** runs when `jf scan` produces `sarif/pixelfrog-xray.sarif`; may be skipped if Advanced Security is not enabled on the repo.

### Build Info: environment, Git, and Conan dependencies

GitHub Actions runs each **job** on a **new VM**. JFrog CLI keeps pending Build Info on that machine until `jf rt build-publish`. Commands like `jf rt build-collect-env` in an **earlier** job do not carry over to the job that publishes.

This workflow therefore:

1. In **`package-binary`**, when **`JF_CONAN_ART_LOCAL_REPO`** is set, installs the Conan **`art`** extension ([conan-extensions](https://github.com/conan-io/conan-extensions)), registers Artifactory with **`conan art:server add`**, runs **`conan install … --format=json`**, then **`conan art:build-info create … --with-dependencies --add-cached-deps`** and **`conan art:build-info upload`** (same pattern as the [Conan JFrog integration doc](https://docs.conan.io/2/integrations/jfrog.html) and public demos). The workflow passes **two** repository keys to **`create`**: your **virtual** (`JF_CONAN_VIRTUAL_REPO` / default) **first**, then the **local** repo. That matters because [conan-extensions](https://github.com/conan-io/conan-extensions/blob/main/extensions/commands/art/cmd_build_info.py) treats a **single** repository as the origin for **every** artifact (no `conanmanifest.txt` probe); dependencies resolved from ConanCenter via Artifactory usually live under the **virtual / remote cache**, not only the local repo — wrong paths cause **missing `.tgz`** warnings and **`set_properties` 400** on upload. **`JF_CONAN_ART_REPO_EXTRA`** adds more keys after those two (deduplicated).
2. If **`JF_CONAN_ART_LOCAL_REPO`** is unset, falls back to **`jf conan install … --build-name` / `--build-number`** in **`package-binary`** so Conan resolution is still tied to the same job as publish.
3. Calls **`jf rt build-publish` with `--collect-env=true` and `--collect-git-info=true`** (and `--build-url` for the Actions run), so the published record includes CI environment variables and the Git revision from the checked-out repo (`fetch-depth: 0` on that checkout improves history for the UI).
4. Avoids **`jf rt build-add-dependencies` on `graph-info.json`** — that attaches **one** generic file, which makes Xray’s SBOM-style views look like a **single** component instead of each Conan package.
5. Optionally uploads a **CycloneDX** file from **`jf audit --format=cyclonedx --sbom --sca`** when your tenant supports it (Advanced Security / audit may be required; the step is best-effort).

Optional repository variable **`JF_PROJECT_KEY`**: when set, it is passed as **`--project`** on **`build-publish`** and on **`conan art:build-info upload`** so Build Info lands under the right Artifactory project.

If you use both **`conan art:build-info upload`** and **`jf rt build-publish`** with the same build name and number, Artifactory usually **merges** module data; if your platform version behaves differently, publish only one path or use **`conan art:build-info append`** (see extension [readme](https://github.com/conan-io/conan-extensions/blob/main/extensions/commands/art/readme_build_info.md)).

---

## 8. Frogbot

JFrog Frogbot scans pull requests for vulnerable dependencies. Minimum setup:

1. Follow the official guide: [Frogbot documentation](https://jfrog.com/help/tools/frogbot).
2. Add JFrog credentials as GitHub secrets (per docs) and commit the generated Frogbot workflow under `.github/workflows/`.

---

## 9. Troubleshooting

### Conan remote authentication failures

- Re-run `conan remote login jfrog-conan <user> -p <token>`.
- Confirm the virtual URL ends with `/api/conan/<repo-key>`.
- Tokens need **permission** to read (and usually deploy, for CI) on Conan repositories.

### Missing packages on ConanCenter

- Ensure the **remote** repository proxies ConanCenter and the **virtual** includes it.
- First resolution may be slow; Artifactory caches packages under the remote cache.

### Build Info missing Git / environment / Conan deps

- Confirm **`package-binary`** completed and **`jf rt build-publish`** ran with **`--collect-git-info`** / **`--collect-env`** (see workflow).
- Ensure **the same `build-name` and `build-number`** are used for `jf conan install` (fallback path), **`conan art:build-info`** (when enabled), and uploads before publish.
- For richer **Conan** rows in Build Info / Xray, set **`JF_CONAN_ART_LOCAL_REPO`** to your **local** Conan repository key. The workflow already prepends your **virtual** repo to **`conan art:build-info create`** so cached dependencies (e.g. from ConanCenter) resolve to real paths in Artifactory. If **`set_properties` / 400** or **missing `.tgz`** persist, add your **Conan remote** (cache) repo key via **`JF_CONAN_ART_REPO_EXTRA`** — some tenants need that for `api/storage/.../conanmanifest.txt` on non-virtual repos.
- If the Xray SBOM view still looks sparse, open **Build Info → Dependencies** for Conan modules; use the uploaded **`pixelfrog-sbom.cdx.json`** (CycloneDX) in Artifactory for a flat component list when `jf audit` succeeds.

### Xray scan timeouts

- Reduce scope (`jf scan` path) or increase runner resources.
- Use `--threads` (see `jf scan --help`) if supported in your CLI version.

### GitHub Advanced Security SARIF upload errors

- **Code scanning** must be enabled for the repo (GitHub Advanced Security / GHAS).
- SARIF must be valid; failed `jf scan` leaves no file and upload is skipped.
- `upload-sarif` uses `continue-on-error: true` so demos do not die on optional GHAS.

---

## 10. Tests

```bash
cd build/Release
ctest --output-on-failure
# or
./pixelfrog_tests
./pixelfrog_tests -r JUnit::out=results.xml
```

---

## Licence

Copyright © contributors to PixelFrog.

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or** (at your option) **any later version**.

This program is distributed in the hope that it will be useful, but **without any warranty**; without even the implied warranty of **merchantability** or **fitness for a particular purpose**. See the [GNU General Public License v3.0](LICENSE) for full terms.

For JFrog demos, publishing the app under GPL-3.0 helps show **licence detection** on your own source and artefacts alongside dependency graphs.
