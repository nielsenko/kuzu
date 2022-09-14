create node table person (ID INT64, fName STRING, gender INT64, isStudent BOOLEAN, isWorker BOOLEAN, age INT64, eyeSight DOUBLE, birthdate DATE, registerTime TIMESTAMP, lastJobDuration INTERVAL, workedHours INT64[], usedNames STRING[], courseScoresPerTerm INT64[][], PRIMARY KEY (ID));
create node table organisation (ID INT64, name STRING, orgCode INT64, mark DOUBLE, score INT64, history STRING, licenseValidInterval INTERVAL, rating DOUBLE, PRIMARY KEY (ID));
create rel table knows (FROM person TO person, date DATE, meetTime TIMESTAMP, validInterval INTERVAL, comments STRING[], MANY_MANY);
create rel table studyAt (FROM person TO organisation, year INT64, places STRING[], MANY_ONE);
create rel table workAt (FROM person TO organisation, year INT64, MANY_ONE);
create rel table meets (FROM person TO person, MANY_ONE);