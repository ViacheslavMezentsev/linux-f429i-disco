# Карта источников от 04.11.2026

| Источник и ревизия | Исходный путь | Использование и лицензия |
| --- | --- | --- |
| [schoolbell](https://github.com/ViacheslavMezentsev/schoolbell/tree/555ce5bc3f6915d1f5d481337b9e22f55a168558) | AGENTS.md, README.md, TODO.md, CHANGELOG.md, docs/{WORKFLOW,BUILD}.md, docs/requirements/TECHNICAL_SPECIFICATION.md, .github/workflows/{ci,pages}.yml | MIT. Образец структуры, подписи и сверки SHA. Зафиксированная ревизия ТЗ 1.112, рабочая копия соседа содержит более поздние незакоммиченные правки. |
| [demo-stm32-skills](https://github.com/ViacheslavMezentsev/demo-stm32-skills/tree/1ceca9146df2442ae2d265e9db1be3bdca67762c) | embedded-tech-spec/{SKILL.md,assets/template.md,references/sections.md,scripts/check_spec.py}, LICENSE | MIT. Методика ревизий и TC. Скрипт проверки скопирован без изменений, его лицензия в tools/LICENSE. |
| [stm32-gdbtest](https://github.com/ViacheslavMezentsev/stm32-gdbtest/tree/3e5905f93912fffcaa2cf295935daf61fa3976b3) | README.md, skills/stm32-gdbtest-{integrate,run}/SKILL.md | MIT. Возможный инструмент аппаратной проверки по символам ELF. |
| [Running Linux 6.14 on stm32f429](https://hackmd.io/@yawu/HkZ2pm22yx) | Статья HackMD, просмотр 04.11.2026, Git SHA не предоставлен | Пример маршрута Buildroot → xipImage и DTB → SD через SPI → загрузка. Лицензия не установлена, материал не копировался. |

Для собственных материалов предложена MIT, решение принимает владелец. При выборе Linux, загрузчика, rootfs и бинарных прошивок инвентаризировать каждый компонент отдельно: версия или SHA, исходный путь, лицензия и условия поставки. Инструменты ST на ПК установлены отдельно.
