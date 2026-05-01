FROM devkitpro/devkitppc:latest

RUN dkp-pacman -Syu --noconfirm --needed wiiu-dev

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITPPC=/opt/devkitpro/devkitPPC
ENV PATH=/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin:/opt/devkitpro/portlibs/wiiu/bin:${PATH}

WORKDIR /workspace
