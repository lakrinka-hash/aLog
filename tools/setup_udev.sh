#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_RULES="${SCRIPT_DIR}/60-openocd.rules"
SYSTEM_RULES="/etc/udev/rules.d/60-openocd.rules"

echo '===================================================================='
echo '         Проверка конфигурации udev правил для OpenOCD'
echo '===================================================================='
echo ''

if [ ! -f "$SYSTEM_RULES" ]; then
    echo 'Правила udev для OpenOCD в системе НЕ найдены.'
    read -p "Установить правила udev из проекта? (Y/N): " choice
    case "$choice" in
        [yY][eE][sS]|[yY])
            echo 'Установка правил udev...'
            sudo cp "$PROJECT_RULES" "$SYSTEM_RULES" && \
            sudo udevadm control --reload-rules && \
            sudo udevadm trigger
            echo 'Правила udev успешно установлены.'
            ;;
        *)
            echo 'Установка отменена. Отладка может работать некорректно.'
            ;;
    esac
    exit 0
fi

PROJECT_HASH=$(md5sum "$PROJECT_RULES" | awk '{print $1}')
SYSTEM_HASH=$(md5sum "$SYSTEM_RULES" | awk '{print $1}')

if [ "$PROJECT_HASH" = "$SYSTEM_HASH" ]; then
    echo 'Правила udev уже установлены и полностью совпадают с проектом.'
    read -p "Принудительно переустановить/перезапустить их? (Y/N): " choice
    case "$choice" in
        [yY][eE][sS]|[yY])
            echo 'Перезапись и перезапуск правил udev...'
            sudo cp "$PROJECT_RULES" "$SYSTEM_RULES" && \
            sudo udevadm control --reload-rules && \
            sudo udevadm trigger
            echo 'Правила udev успешно обновлены.'
            ;;
        *)
            echo 'Правила udev не изменены.'
            ;;
    esac
else
    echo 'Обнаружено различие между системными и проектными правилами udev.'
    echo '--------------------------------------------------------------------'
    echo ' Разница (--- система, +++ проект):'
    diff -u "$SYSTEM_RULES" "$PROJECT_RULES" || true
    echo '--------------------------------------------------------------------'
    
    read -p "Затереть системные правила udev версией из проекта? (Y/N): " choice
    case "$choice" in
        [yY][eE][sS]|[yY])
            echo 'Обновление правил udev...'
            sudo cp "$PROJECT_RULES" "$SYSTEM_RULES" && \
            sudo udevadm control --reload-rules && \
            sudo udevadm trigger
            echo 'Системные правила udev успешно обновлены.'
            ;;
        *)
            echo 'Обновление системных правил udev отменено.'
    esac
fi

echo ''
echo '===================================================================='
echo ''