# Building the project
Requirements:
- Windows.
- Visual Studio 2022.
- PostgreSQL 9.6

After installing the required programs, open `msvc/archlord.sln`, set platform to `x64` and build the solution.

# Installation
## Preparing the database
PostgreSQL 9.6 is required to run the server.
Later versions of PostgreSQL may or may not work.
After installing PostgreSQL, do not forget to add its `bin` directory to your path. 

After installation, open a command prompt and make sure that your path is properly setup by typing:
```
psql --version
```
You should get something similar to `psql (PostgreSQL) 9.6.9`.
Next, type following to create database and user:
```
createuser -U postgres aluser
createdb -U postgres archlord
```
Access PostgreSQL shell with:
```
psql -U postgres (type in your password)
```
Provide privileges to user:
```
ALTER USER aluser WITH ENCRYPTED PASSWORD 'pwdpwd';
GRANT ALL PRIVILEGES ON DATABASE archlord TO aluser;
```
Now that the database and user are ready, we can create required tables.
First, logout with:
```
\q
```
Login with:
```
psql -U aluser -d archlord
```
Create required tables:
```
CREATE TABLE accounts (
	account_id VARCHAR(50) PRIMARY KEY NOT NULL,
	data BYTEA
);

CREATE TABLE guilds (
	guild_id VARCHAR(50) PRIMARY KEY NOT NULL,
	data BYTEA
);
```

## Server configuration
Open `(repo)/config` with any text editor.
`ServerIniDir` and `ServerNodePath` are relative to project directory so they can be skipped. 
`..ServerIP` should be changed to your machine's IP address.
`DBName`, `DBUser` and `DBPassword` can be skipped as long as you have followed instructions above.

## Creating a test account
After preparing the database, compiling the project and editing configuration file, it is now possible to create an account and enter the game.
