RED='\033[31m'
GREEN='\033[38;5;46m'
YELLOW='\033[1;33m'
BLUE='\033[34m'
CYAN='\033[36m'
BOLD='\033[1m'
WHITE='\033[37m'
BLACK='\033[30m'
YELLOW_GRAD1='\033[38;5;226m'
YELLOW_GRAD2='\033[38;5;220m'
YELLOW_GRAD3='\033[38;5;214m'
YELLOW_GRAD4='\033[38;5;208m'
YELLOW_GRAD5='\033[38;5;202m'
BLUE_GRAD1='\033[38;5;51m' 
BLUE_GRAD2='\033[38;5;45m'
BLUE_GRAD3='\033[38;5;39m'
BLUE_GRAD4='\033[38;5;33m'
BLUE_GRAD5='\033[38;5;27m'
NC='\033[0m' # No Color

BLACK_BG='\033[40m'
RED_BG='\033[41m'
GREEN_BG='\033[42m'
YELLOW_BG='\033[43m'
BLUE_BG='\033[44m'
MAGENTA_BG='\033[45m'
CYAN_BG='\033[46m'
GRAY_BG='\033[100m'
WHITE_BG='\033[47m'
BG_RESET='\033[49m'


    echo -e "  ${YELLOW_GRAD4}╭─────────────────────────────────────────────────────────────────╮ "
    echo -e "  ${YELLOW_GRAD4}│  ${CYAN}1)${NC} Build new splash            ${CYAN}5)${NC} Splash current status        ${YELLOW_GRAD4}│"
    echo -e "  ${YELLOW_GRAD4}│  ${CYAN}2)${NC} Install xbs_* binary        ${CYAN}6)${NC} Uninstall ALL splash systems ${YELLOW_GRAD4}│"
    echo -en "  ${YELLOW_GRAD4}│  ${CYAN}3)${NC} Extract xbs_* binary        "
    if [[ $LIBDRM_AVAILABLE -eq 1 ]]; then
        if [[ $USE_DRM -eq 1 ]]; then
           echo -e "${CYAN}T)${NC} Toggle mode (current: ${GREEN}DRM${NC})   ${YELLOW_GRAD4}│"
        else
           echo -e "${CYAN}T)${NC} Toggle mode (current: ${GREEN}fbdev${NC}) ${YELLOW_GRAD4}│"
        fi
    fi
    echo -e "  ${YELLOW_GRAD4}│  ${CYAN}4)${NC} Uninstall xbootsplash       ${CYAN}Q)${NC} Quit                         ${YELLOW_GRAD4}│"
    echo -e "  ${YELLOW_GRAD4}╰─────────────────────────────────────────────────────────────────╯ "
    echo -en " ${YELLOW}  ➤ Select option [${CYAN}1${YELLOW}]: ${NC}"


     ┕━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┙

USE_DRM=1 /home/cdef/_PROJETS/bootsplash/CLI_src/build_anim.sh -n xbs_win11 \
-m 0 -x 0 -y 250 -d 33 -c 000000 \
-l 1 \
/home/cdef/_PROJETS/bootsplash/win11/
╭───────────────────────────────────────────────────────────────╮
│  1) Full loop     - Play 0→N, then restart from first         │
│  2) No loop       - Play once, stay on last frame             │
│  3) Partial loop  - Play 0→N, then loop from frame X          │
│  I) Invert frames direction - current: 0→N (normal)           │
┕━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┙

