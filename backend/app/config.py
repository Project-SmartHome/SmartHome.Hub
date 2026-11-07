
import os


class BaseConfig:
    SECRET_KEY = os.environ.get("SECRET_KEY")

    SQLALCHEMY_DATABASE_URI = os.environ.get(
        "DATABASE_URL",
        "sqlite:///smarthome.db"
    )

    SQLALCHEMY_TRACK_MODIFICATIONS = False

    VERSION = os.environ.get("APP_VERSION", "0.1.0")


class DevelopmentConfig(BaseConfig):
    DEBUG = True


class ProductionConfig(BaseConfig):
    DEBUG = False


class TestingConfig(BaseConfig):
    TESTING = True
    SQLALCHEMY_DATABASE_URI = "sqlite:///:memory:"


def get_config(config_name=None):
    configurations = {
        "development": DevelopmentConfig,
        "production": ProductionConfig,
        "testing": TestingConfig
    }

    environment = config_name or os.environ.get(
        "FLASK_ENV",
        "development"
    )

    return configurations.get(environment, DevelopmentConfig)
