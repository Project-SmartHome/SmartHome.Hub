FROM python:3.12-slim
WORKDIR /backend

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

COPY ["requirements.txt", "./"]
RUN pip install --no-cache-dir -r requirements.txt
COPY . .

# Injected by the release workflow; read by app/config.py
ARG APP_VERSION=0.0.0-dev
ENV APP_VERSION=${APP_VERSION}

EXPOSE 5000
CMD ["python", "run.py"] 