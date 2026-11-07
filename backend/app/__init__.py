
from flask import Flask, jsonify

from app.config import get_config
# from app.extensions import db, migrate, cors


def create_app(config_name=None):
    """Create and configure the SmartHome Hub application."""

    app = Flask(__name__)

    # Load configuration
    app.config.from_object(get_config(config_name))

    # Initialize extensions
    # initialize_extensions(app)

    # Register API routes
    # register_blueprints(app)

    # Register error handlers
    register_error_handlers(app)

    # Health endpoint
    @app.get("/api/health")
    def health():
        return jsonify({
            "status": "healthy",
            "service": "smarthome-hub",
            "version": app.config.get("VERSION", "0.1.0")
        }), 200

    return app


def initialize_extensions(app):
    """Initialize Flask extensions."""

    db.init_app(app)
    migrate.init_app(app, db)
    cors.init_app(app)


def register_blueprints(app):
    """Register application API blueprints."""

    from app.api.devices import devices_bp
    from app.api.telemetry import telemetry_bp
    from app.api.gateway import gateway_bp
    from app.api.firmware import firmware_bp

    app.register_blueprint(
        devices_bp,
        url_prefix="/api/devices"
    )

    app.register_blueprint(
        telemetry_bp,
        url_prefix="/api/telemetry"
    )

    app.register_blueprint(
        gateway_bp,
        url_prefix="/api/gateway"
    )

    app.register_blueprint(
        firmware_bp,
        url_prefix="/api/firmware"
    )


def register_error_handlers(app):
    """Register global error handlers."""

    @app.errorhandler(404)
    def not_found(error):
        return jsonify({
            "error": "Not found",
            "message": "The requested resource does not exist."
        }), 404

    @app.errorhandler(500)
    def internal_server_error(error):
        return jsonify({
            "error": "Internal server error"
        }), 500
