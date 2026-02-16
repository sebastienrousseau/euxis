# Euxis Enterprise — Agent Workforce Container
FROM python:3.11-slim

# Install system dependencies for Bash scripts and Vector DB
RUN apt-get update && apt-get install -y \
    curl \
    git \
    build-essential \
    libportaudio2 \
    && rm -rf /var/lib/apt/lists/*

# Set up the Euxis environment
ENV EUXIS_HOME=/root/.euxis
WORKDIR /app

# Copy requirement files first for caching
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy the entire Euxis brain and scripts
COPY . $EUXIS_HOME
COPY cli/bin/* /usr/local/bin/

# Ensure everything is executable
RUN chmod +x /usr/local/bin/euxis*

# Default command: Start the Euxis Daemon
CMD ["euxis-daemon"]
