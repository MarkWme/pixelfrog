# PixelFrog — operator quick start

Command-line C++17 demo that loads JPEG/PNG, applies filters (greyscale, Gaussian blur, Sobel edges, brighten/darken), and writes an output image. The project is built for **JFrog SaaS + Conan + GitHub Actions** walkthroughs (Artifactory, Xray, Build Info, Release Bundles).

**Time budget:** about 45–60 minutes for Artifactory + secrets + first green CI run, assuming a clean JFrog trial tenant.

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

## 3. GitHub Actions secrets (Section 5.2)

In the GitHub repo: **Settings** → **Secrets and variables** → **Actions** → **New repository secret**:

| Secret            | Purpose |
|-------------------|--------|
| `JF_URL`          | Platform URL, e.g. `https://acme.jfrog.io` (no trailing slash). |
| `JF_ACCESS_TOKEN` | Access token with read/write to Conan + generic repos used in the workflow. |
| `JF_CONAN_USER` *(optional)* | Username for `conan remote login` (defaults to `admin` if unset). |

The workflow derives the Conan client remote as:

`${JF_URL}/artifactory/api/conan/conan-virtual`

If your virtual repository key is **not** `conan-virtual`, fork the workflow and change that path segment.

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

Demo / sample code — use and adapt for JFrog customer demos under your own policies.
