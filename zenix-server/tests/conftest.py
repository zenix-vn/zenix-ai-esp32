import os
import tempfile

# Phai dat truoc khi import app (config doc bien moi truong luc import).
os.environ["ZENIX_DATA_DIR"] = tempfile.mkdtemp(prefix="zenix-test-")
os.environ["ZENIX_PUBLIC_URL"] = "https://dev.example.com"
