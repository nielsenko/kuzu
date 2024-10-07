COPY City from "dataset/lsqb-sf01/City.csv" (HEADER=true);
COPY Comment from "dataset/lsqb-sf01/Comment.csv" (HEADER=true);
COPY Company from "dataset/lsqb-sf01/Company.csv" (HEADER=true);
COPY Continent from "dataset/lsqb-sf01/Continent.csv" (HEADER=true);
COPY Country from "dataset/lsqb-sf01/Country.csv" (HEADER=true);
COPY Forum from "dataset/lsqb-sf01/Forum.csv" (HEADER=true);
COPY Person from "dataset/lsqb-sf01/Person.csv" (HEADER=true);
COPY Post from "dataset/lsqb-sf01/Post.csv" (HEADER=true);
COPY Tag from "dataset/lsqb-sf01/Tag.csv" (HEADER=true);
COPY TagClass from "dataset/lsqb-sf01/TagClass.csv" (HEADER=true);
COPY University from "dataset/lsqb-sf01/University.csv" (HEADER=true);
COPY City_isPartOf_Country from "dataset/lsqb-sf01/City_isPartOf_Country.csv" (HEADER=true);
COPY Comment_hasCreator_Person from "dataset/lsqb-sf01/Comment_hasCreator_Person.csv" (HEADER=true);
COPY Comment_hasTag_Tag from "dataset/lsqb-sf01/Comment_hasTag_Tag.csv" (HEADER=true);
COPY Comment_isLocatedIn_Country from "dataset/lsqb-sf01/Comment_isLocatedIn_Country.csv" (HEADER=true);
COPY Comment_replyOf_Comment from "dataset/lsqb-sf01/Comment_replyOf_Comment.csv" (HEADER=true);
COPY Comment_replyOf_Post from "dataset/lsqb-sf01/Comment_replyOf_Post.csv" (HEADER=true);
COPY Company_isLocatedIn_Country from "dataset/lsqb-sf01/Company_isLocatedIn_Country.csv" (HEADER=true);
COPY Country_isPartOf_Continent from "dataset/lsqb-sf01/Country_isPartOf_Continent.csv" (HEADER=true);
COPY Forum_containerOf_Post from "dataset/lsqb-sf01/Forum_containerOf_Post.csv" (HEADER=true);
COPY Forum_hasMember_Person from "dataset/lsqb-sf01/Forum_hasMember_Person.csv" (HEADER=true);
COPY Forum_hasModerator_Person from "dataset/lsqb-sf01/Forum_hasModerator_Person.csv" (HEADER=true);
COPY Forum_hasTag_Tag from "dataset/lsqb-sf01/Forum_hasTag_Tag.csv" (HEADER=true);
COPY Person_hasInterest_Tag from "dataset/lsqb-sf01/Person_hasInterest_Tag.csv" (HEADER=true);
COPY Person_isLocatedIn_City from "dataset/lsqb-sf01/Person_isLocatedIn_City.csv" (HEADER=true);
COPY Person_knows_Person from "dataset/lsqb-sf01/Person_knows_Person.csv" (HEADER=true);
COPY Person_likes_Comment from "dataset/lsqb-sf01/Person_likes_Comment.csv" (HEADER=true);
COPY Person_likes_Post from "dataset/lsqb-sf01/Person_likes_Post.csv" (HEADER=true);
COPY Person_studyAt_University from "dataset/lsqb-sf01/Person_studyAt_University.csv" (HEADER=true);
COPY Person_workAt_Company from "dataset/lsqb-sf01/Person_workAt_Company.csv" (HEADER=true);
COPY Post_hasCreator_Person from "dataset/lsqb-sf01/Post_hasCreator_Person.csv" (HEADER=true);
COPY Post_hasTag_Tag from "dataset/lsqb-sf01/Post_hasTag_Tag.csv" (HEADER=true);
COPY Post_isLocatedIn_Country from "dataset/lsqb-sf01/Post_isLocatedIn_Country.csv" (HEADER=true);
COPY Tag_hasType_TagClass from "dataset/lsqb-sf01/Tag_hasType_TagClass.csv" (HEADER=true);
COPY TagClass_isSubclassOf_TagClass from "dataset/lsqb-sf01/TagClass_isSubclassOf_TagClass.csv" (HEADER=true);
COPY University_isLocatedIn_City from "dataset/lsqb-sf01/University_isLocatedIn_City.csv" (HEADER=true);
