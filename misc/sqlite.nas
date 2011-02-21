var db = sqlite.open("testtesttest.sqlite");

# Reset. Ignore errors by using call().
call(func { sqlite.exec(db, "drop table foo");
	    sqlite.exec(db, "drop table bar"); }, [], []);

var stmt = sqlite.prepare(db, "create table foo (id integer, name text)");
sqlite.exec(db, stmt);
sqlite.finalize(stmt);

sqlite.exec(db, "insert into foo values (?, ?)", 1, "a");
sqlite.exec(db, "insert into foo values (?, ?)", 2, "b");
sqlite.exec(db, "insert into foo values (?, ?)", 3, "c");
sqlite.exec(db, "insert into foo values (?, ?)", 4, "d");

var stmt = sqlite.prepare(db, "insert into foo values (?, ?)");
sqlite.exec(db, stmt, 5, "e");
sqlite.exec(db, stmt, 6, "f");
sqlite.exec(db, stmt, 7, "g");
sqlite.exec(db, stmt, 8, "h");

# Query foo using the callback feature to copy the table onto "bar"
sqlite.exec(db, "create table bar (id integer, name text)");
var stmt = sqlite.prepare(db, "insert into bar values (?, ?)");
sqlite.exec(db, "select * from foo",
	    func(row) { sqlite.exec(db, stmt, row.id, row.name) });

# Now query bar using the vector result
foreach(row; sqlite.exec(db, "select * from bar"))
    print(sprintf("id %d name '%s'\n", row.id, row.name));

sqlite.close(db);
