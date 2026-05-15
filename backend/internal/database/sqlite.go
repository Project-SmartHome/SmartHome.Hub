package database

import (
	"database/sql"
	"log"
	"os"
	"path/filepath"
	"smarthome/hub/internal/config"

	_ "modernc.org/sqlite"
)

var DB *sql.DB

func Connect() {
	db, err := Open(config.DefaultDatabasePath())
	if err != nil {
		log.Fatal(err)
	}

	DB = db
}

func Open(path string) (*sql.DB, error) {

	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		return nil, err
	}

	db, err := sql.Open(
		"sqlite",
		path,
	)

	if err != nil {
		return nil, err
	}

	if err := db.Ping(); err != nil {
		db.Close()
		return nil, err
	}

	_, err = db.Exec(`
        PRAGMA journal_mode=WAL;

        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS rooms (
            id TEXT PRIMARY KEY,
            room_urn TEXT NOT NULL UNIQUE,
            name TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS user_rooms (
            id TEXT PRIMARY KEY,
            user_id INTEGER NOT NULL,
            room_guid TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
            UNIQUE(user_id, room_guid),
            FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE,
            FOREIGN KEY(room_guid) REFERENCES rooms(id) ON DELETE RESTRICT
        );
    `)

	if err != nil {
		db.Close()
		return nil, err
	}

	if err := seedRooms(db); err != nil {
		db.Close()
		return nil, err
	}

	return db, nil
}

func seedRooms(db *sql.DB) error {
	_, err := db.Exec(`
		INSERT INTO rooms (id, room_urn, name) VALUES
			('1f1fe586-afc6-447b-ba8b-a90bba9be12a', 'urn:room:1', 'Art Room'),
			('b8ac51c3-d2f9-4d6d-8bd9-075a303db246', 'urn:room:2', 'Bathroom'),
			('f2e45f12-f82b-41f0-8cfe-8ca537eb577c', 'urn:room:3', 'Bedroom'),
			('5b2cce70-bad1-4dde-a6a4-104e04d1ff5f', 'urn:room:5', 'Computer Room'),
			('551afbef-af3e-497a-a7b5-e69e68138645', 'urn:room:4', 'Conference Room'),
			('938d3659-dab4-4995-88d2-b584fd9093cd', 'urn:room:6', 'Dining Room'),
			('2d937122-aa4f-4209-ba3e-c50aa33334eb', 'urn:room:7', 'Family Room'),
			('616ff73b-8608-460f-bcca-b355dd8eb451', 'urn:room:8', 'Garage'),
			('81b2b0a3-bdb8-4a33-b205-111a06ce736d', 'urn:room:9', 'Guest Room'),
			('7debd87c-31ff-45ff-8fb4-3fc687fdfee5', 'urn:room:10', 'Hallway'),
			('350052df-f388-4b14-94d9-400c68a02155', 'urn:room:11', 'Home Office'),
			('369c3116-f3c2-4041-9778-6fd2ad222124', 'urn:room:12', 'Kids Room'),
			('5584409a-17a5-4307-b000-28dbe695a9a4', 'urn:room:13', 'Kitchen'),
			('10c6066c-fcf1-4bf3-88b9-b652922b892a', 'urn:room:14', 'Laundry Room'),
			('d3e5dc30-81d8-4885-bf5f-8c8b61a34eb8', 'urn:room:15', 'Library'),
			('011f86b2-7b4b-46ad-809b-8c802c4d8ce9', 'urn:room:16', 'Living Room'),
			('e7cc063a-e6f0-4a9a-82d0-1faeea73197d', 'urn:room:17', 'Pantry'),
			('4339ed21-bd97-476e-92ef-953f9ba415a9', 'urn:room:18', 'Storage Room'),
			('ece8e2a0-b333-429c-9266-0e3ffa0525ab', 'urn:room:19', 'Study'),
			('9d84ce32-b289-4de6-9ef7-faf578da2c49', 'urn:room:20', 'Utility Room')
		ON CONFLICT(id) DO UPDATE SET
			room_urn = excluded.room_urn,
			name = excluded.name;
	`)

	return err
}
