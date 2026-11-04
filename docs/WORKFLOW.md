# Рабочий цикл

Начать с AGENTS.md. В WSL проверить pwd, git status -sb, git log, git remote -v и git ls-remote --symref origin HEAD refs/heads/main. При наличии истории выполнить git fetch origin и обновить чистую main через git merge --ff-only origin/main. От неё создать codex/<задача>.

После согласования поведения обновить ТЗ, TC и матрицу. Реализовать изменение, выполнить профильные тесты и сборку, сверить TODO и CHANGELOG RU/EN. Осмотреть git diff, затем git add -- с конкретными путями и git diff --cached --check и git diff --cached.

Команды PowerShell из корня рабочей копии:
```powershell
$repo = (Get-Location).ProviderPath
$safe = 'safe.directory=%(prefix)/' + $repo.Replace('\','/')
function wg { git -c $safe -C $repo @args }
wg config --get-regexp '^(user\.(name|email|signingkey)|gpg\.(format|ssh\.allowedsignersfile))$'
wg commit -S -m 'type: describe the change'
wg verify-commit HEAD
wg log -1 '--format=%h %G? %s'
wsl git status -sb
wsl git diff --summary
wsl git ls-files --stage
```
Продолжать только после успешной проверки и G. При сбое оставить индекс. Передать владельцу SHA и результаты. После «слил» выполнить git fetch origin, сравнить git rev-parse main origin/main и git ls-remote --symref origin HEAD. Проверить headSha и conclusion конкретного запуска через gh run view RUN_ID --json headSha,conclusion. Если CI ещё не настроен, отметить проверку открытой.

Host checks использует Clang/LLD из ubuntu-24.04 для сборки автономной ARM-пробы. Локальный make check по умолчанию использует GNU Arm. Ограничение job в 5 минут относится к выполнению на раннере; ожидание очереди GitHub учитывается отдельно.
