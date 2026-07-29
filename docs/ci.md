# CI / CD (Gitea Actions)

Workflows: [`.gitea/workflows/ci.yml`](../.gitea/workflows/ci.yml) (mirror under `.github/workflows/`).

## Common failures on Gitea `act_runner`

| Error | Cause | Fix in this repo |
|-------|--------|------------------|
| `exec: "node": not found` (126) | Job `container: espressif/idf` + `actions/checkout` | No job-level IDF container; checkout on host |
| `Bind mount failed: '/workspace/...'` (125) | Nested `docker run -v $GITHUB_WORKSPACE` — path exists only inside the job container | [`tools/ci/idf-docker.sh`](../tools/ci/idf-docker.sh) uses `--volumes-from` |

## How builds run

1. `actions/checkout` on the runner (needs Node on the **job** environment).
2. `tools/ci/idf-docker.sh` starts `espressif/idf:v5.5.1`:
   - **Inside act_runner job container:** `--volumes-from <self>` + `-w $GITHUB_WORKSPACE/...`
   - **Bare-metal runner:** `-v $GITHUB_WORKSPACE:/project`

## Jobs

| Job | What |
|-----|------|
| `unity-tests` | `test_apps/embed_unity` |
| `firmware` | main app, `partitions.csv` |
| `firmware-ota` | `sdkconfig.defaults` + `sdkconfig.defaults.ota` |

## Runner setup

1. Enable Actions on the repo.
2. `act_runner` with Docker socket access.
3. Label `ubuntu-latest`.
4. Ability to pull `espressif/idf:v5.5.1`.

If `--volumes-from` still fails, check that the runner’s Docker mode allows containers to see sibling containers (`docker inspect $(cat /etc/hostname)` from a debug step).
