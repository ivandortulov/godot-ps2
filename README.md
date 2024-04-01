# Godot Engine - Playstation 2 Homebrew Port

## Building

To build the export template for the Playstation 2, follow these steps:

1. Clone this repo
1. Build the docker image from the Dockerfile inside the repository

   ```bash
   docker build -t godot-ps2:latest .
   ```

1. Start the docker image by creating a container:

   ```bash
   docker compose up -d
   ```

1. Run SCons inside the docker container:

   ```bash
   docker exec -t godot-ps2-compiler sh -c 'scons platform=ps2'
   ```
