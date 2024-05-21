#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cmath>

using namespace std;

class Record {
public:
    int id, manager_id; // Employee ID and their manager's ID
    string bio, name; // Fixed length string to store employee name and biography

    Record(vector<string> &fields) {
        id = stoi(fields[0]);
        name = fields[1];
        bio = fields[2];
        manager_id = stoi(fields[3]);
    }

    // Function to get the size of the record
    // Returns the size of 5 offset ints, 2 ints for id & managerid, 2 ints for storing name & bio length plus their sizes
    int get_size(){
        return sizeof(int) * 9 + name.size() + bio.size();
    }

    // Serialize the record for writing to file
    string serialize() const {
        ostringstream oss;
        int name_len = name.size();
        int bio_len = bio.size();

        // Make offsets for the record structure (offset1,offset2,offset3,offset4,offset5,id,namelength+name,biolength+bio,managerid)
        int offset1 = sizeof(int) * 5; // field1 starts after the 5 offset ints
        int offset2 = offset1 + sizeof(id); 
        int offset3 = offset2 + sizeof(name_len) + name_len; // accoutnign for the int that says the length of str + the str itself
        int offset4 = offset3 + sizeof(bio_len) + bio_len;
        int offset5 = offset4 + sizeof(manager_id);

        // Serialize offsets
        oss.write(reinterpret_cast<const char*>(&offset1), sizeof(offset1));
        oss.write(reinterpret_cast<const char*>(&offset2), sizeof(offset2));
        oss.write(reinterpret_cast<const char*>(&offset3), sizeof(offset3));
        oss.write(reinterpret_cast<const char*>(&offset4), sizeof(offset4));
        oss.write(reinterpret_cast<const char*>(&offset5), sizeof(offset5));

        // Serialize record data
        oss.write(reinterpret_cast<const char*>(&id), sizeof(id));
        oss.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        oss.write(name.c_str(), name.size());
        oss.write(reinterpret_cast<const char*>(&bio_len), sizeof(bio_len));
        oss.write(bio.c_str(), bio.size());
        oss.write(reinterpret_cast<const char*>(&manager_id), sizeof(manager_id));

        return oss.str();
    }

    void print() const {
        cout << "\tID: " << id << "\n";
        cout << "\tNAME: " << name << "\n";
        cout << "\tBIO: " << bio << "\n";
        cout << "\tMANAGER_ID: " << manager_id << "\n";
    }
};

class Page {
public:
    vector<Record> records; // Data_Area containing the records
    vector<pair<int, int>> slot_directory; // Slot directory containing offset and size of each record
    int cur_size = 0; // Current size of the page
    int overflowPointerIndex; // Offset of overflow page, set to -1 by default
    int n_slots = 0; // holds the current number of entries in the slot directory
    int pointer_to_free_space = 0; // points to the start of free space in the page

    // Constructor
    Page() : overflowPointerIndex(-1) {}

    // Function to insert a record into the page
    bool insert_record_into_page(Record r) {
        int slot_directory_entry_size = sizeof(int) * 2;
        if (cur_size + r.get_size() >= 4096) { // Check if page size limit exceeded
            return false; // Cannot insert the record into this page
        } else {
            // Add Record to page
            records.push_back(r);

            // Get info for slot directory
            int record_offset = pointer_to_free_space;
            int record_length = r.get_size();

            // update slot directory information
            slot_directory.push_back(make_pair(record_offset, record_length));
            n_slots++;

            // Update the new size of the page with the record size & slot directory size
            cur_size += record_length;
            cur_size += slot_directory_entry_size;

            // Update the current pointer to the free space, which is the end of the space the records take up
            pointer_to_free_space += record_length;

            return true;
        }
    }

    void clear(){
        records.clear();
        slot_directory.clear();
        cur_size = 0; // holds the current size of the 
        n_slots = 0; // holds the current number of entries in the slot directory
        pointer_to_free_space = 0; // points to the start of free space in the page
    }

    // Function to write the page to a binary output stream. You may use
    void write_into_data_file(ostream &out) const {
        char page_data[4096] = {0}; // Buffer to hold page data
        int offset = 0;

        // Write records into page_data buffer
        for (const auto &record: records) {
            string serialized = record.serialize();
            memcpy(page_data + offset, serialized.c_str(), serialized.size());
            offset += serialized.size();
        }

        // TODO:
        //  - Write slot_directory in reverse order into page_data buffer.
        //  - Write overflowPointerIndex into page_data buffer.
        //  You should write the first entry of the slot_directory, which have the info about the first record at the bottom of the page, before overflowPointerIndex.

        // store the pointer to free space as well as the number of slots used in the slot directory
        int ptr_location = 4096 - sizeof(int);
        int slot_num_location = 4096 - sizeof(int)*2;

        memcpy(page_data + ptr_location, &pointer_to_free_space, sizeof(int));
        memcpy(page_data + slot_num_location, &n_slots, sizeof(int));

        // Write the overflow pointer index before the slot directory
        int overflow_pointer_location = slot_num_location - sizeof(int);
        memcpy(page_data + overflow_pointer_location, &overflowPointerIndex, sizeof(int));

        // slot directory starts right after overflow pointer (reverse from the end)
        int slot_directory_location = overflow_pointer_location;
        // store the slot directory at the end of the page data, but before the ptr and slot num
        for (const auto& slots : slot_directory) {
            // insert the slot directory information into the page_data
            slot_directory_location -= sizeof(int) * 2;

            // Store first value in vector pair
            memcpy(page_data + slot_directory_location, &slots.first, sizeof(int));
            // Store seocnd value right after the first
            memcpy(page_data + slot_directory_location+sizeof(int), &slots.second, sizeof(int));
        }

        // Write the page_data buffer to the output stream
        out.write(page_data, sizeof(page_data));
    }

    // Function to read a page from a binary input stream
    bool read_from_data_file(istream &in) {
        char page_data[4096] = {0}; // Buffer to hold page data
        in.read(page_data, 4096); // Read data from input stream

        streamsize bytes_read = in.gcount();
        if (bytes_read == 4096) {
            // TODO: Process data to fill the records, slot_directory, and overflowPointerIndex

            int pointer_offset = 4096 - sizeof(int);
            int num_slots_offset = 4096 - sizeof(int) * 2;
            int overflow_pointer_offset = 4096 - sizeof(int) * 3;

            // Get those values from the page data
            memcpy(reinterpret_cast<char *>(&pointer_to_free_space), page_data + pointer_offset, sizeof(int));
            memcpy(reinterpret_cast<char *>(&n_slots), page_data + num_slots_offset, sizeof(int));
            memcpy(reinterpret_cast<char *>(&overflowPointerIndex), page_data + overflow_pointer_offset, sizeof(int));

            // Fill slot directory
            int slot_directory_location = overflow_pointer_offset;
            for (int i = 0; i < n_slots; i++) {
                slot_directory_location -= sizeof(int) * 2;
                int record_offset = 0;
                int record_length = 0;

                memcpy(reinterpret_cast<char *>(&record_offset), page_data + slot_directory_location, sizeof(int));
                memcpy(reinterpret_cast<char *>(&record_length), page_data + slot_directory_location + sizeof(int), sizeof(int));

                slot_directory[i] = make_pair(record_offset, record_length);
            }

            // Fill record data
            for (int i = 0; i < n_slots; i++) {
                int record_offset = slot_directory[i].first;
                int record_length = slot_directory[i].second;

                // Get the offsets of every field
                int id_offset, name_offset, bio_offset, manager_id_offset, end_offset;
                memcpy(reinterpret_cast<char *>(&id_offset), page_data + record_offset, sizeof(int));
                memcpy(reinterpret_cast<char *>(&name_offset), page_data + record_offset + sizeof(int), sizeof(int));
                memcpy(reinterpret_cast<char *>(&bio_offset), page_data + record_offset + sizeof(int) * 2, sizeof(int));
                memcpy(reinterpret_cast<char *>(&manager_id_offset), page_data + record_offset + sizeof(int) * 3, sizeof(int));
                memcpy(reinterpret_cast<char *>(&end_offset), page_data + record_offset + sizeof(int) * 4, sizeof(int));

                // Get the values at each field

                // Get ID, read as int first
                int id;
                memcpy(reinterpret_cast<char *>(&id), page_data + id_offset + record_offset, sizeof(int));

                // Get Name
                int name_length;
                memcpy(&name_length, page_data + record_offset + name_offset, sizeof(int));
                string name(name_length, '\0');
                memcpy(&name[0], page_data + record_offset + name_offset + sizeof(int), name_length);

                // Get Bio
                int bio_length;
                memcpy(&bio_length, page_data + record_offset + bio_offset, sizeof(int));
                string bio(bio_length, '\0');
                memcpy(&bio[0], page_data + record_offset + bio_offset + sizeof(int), bio_length);

                // Get manager id, read as int first
                int manager_id;
                memcpy(reinterpret_cast<char *>(&manager_id), page_data + manager_id_offset + record_offset, sizeof(int));

                vector<string> record_data = {to_string(id), name, bio, to_string(manager_id)};

                Record r(record_data);
                records.push_back(r);
            }                
            return true;
        }

        if (bytes_read > 0) {
            cerr << "Incomplete read: Expected 4096 bytes, but only read " << bytes_read << " bytes." << endl;
        }

        return false;
    }
};

class HashIndex {
private:
    const size_t maxCacheSize = 1; // Maximum number of pages in the buffer
    const int Page_SIZE = 4096; // Size of each page in bytes
    vector<int> PageDirectory; // Map h(id) to a bucket location in EmployeeIndex(e.g., the jth bucket)
    // can scan to correct bucket using j*Page_SIZE as offset (using seek function)
    // can initialize to a size of 256 (assume that we will never have more than 256 regular (i.e., non-overflow) buckets)
    int nextFreePage; // Next place to write a bucket
    string fileName;
    fstream data_file;

    // Function to compute hash value for a given ID
    int compute_hash_value(int id) {
        int hash_value;

        // TODO: Implement the hash function h = id mod 2^8
        hash_value = id % 256;
        return hash_value;
    }

    // Function to add a new record to an existing page in the index file
    void addRecordToIndex(int pageIndex, Page &page, Record &record) {
        // Open index file in binary mode for updating
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);

        if (!indexFile) {
            cerr << "Error: Unable to open index file for adding record." << endl;
            return;
        }

        // Keep going for every page & potential overflow page
        while(1) {
            // Check if the page doesn't have overflow
            if (page.overflowPointerIndex == -1) {
                // Try inserting a record into this page
                if (page.insert_record_into_page(record)) {
                    indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
                    page.write_into_data_file(indexFile);
                    indexFile.close();
                    return;
                }
                // If inserting fails, then create an overflow page to insert record into
                else {
                    // Create overflow page & set it to the next free page, update overflowIndex
                    Page overflowPage;
                    int overflowPageIndex = nextFreePage;
                    nextFreePage++;
                    page.overflowPointerIndex = overflowPageIndex;

                    // Put our record in this new overflow page
                    overflowPage.insert_record_into_page(record);

                    // Update the index file by inserting our updated page, since we updated it with the overflow page
                    indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
                    page.write_into_data_file(indexFile);

                    // Now update the index file with the overflow page
                    indexFile.seekp(overflowPageIndex * Page_SIZE, ios::beg);
                    overflowPage.write_into_data_file(indexFile);

                    indexFile.close();
                    return;   
                }
            }
            // If page does have overflow, go to that page
            else {
                indexFile.seekg(pageIndex * Page_SIZE, ios::beg);
                Page overflowPage;
                overflowPage.read_from_data_file(indexFile);
                pageIndex = overflowPage.overflowPointerIndex;
            }
        }
    }


public:
    HashIndex(string indexFileName) : nextFreePage(0), fileName(indexFileName), PageDirectory(256, -1) {
        data_file.open(fileName, ios::binary | ios::out | ios::in | ios::trunc);
        if (!data_file.is_open()) {  // Check if the data_file was successfully opened
            cerr << "Failed to open data_file: " << fileName << endl;
            exit(EXIT_FAILURE);  // Exit if the data_file cannot be opened
        }    
    }

    // Function to create hash index from Employee CSV file
    void createFromFile(string csvFileName) {
        // Read CSV file and add records to index
        // Open the CSV file for reading
        ifstream csvFile(csvFileName);

        string line;
        // Read each line from the CSV file
        while (getline(csvFile, line)) {
            // Parse the line and create a Record object
            stringstream ss(line);
            string item;
            vector<string> fields;
            while (getline(ss, item, ',')) {
                fields.push_back(item);
            }
            Record record(fields);

            // TODO:
            //   - Compute hash value for the record's ID using compute_hash_value() function.
            int hashed_id = compute_hash_value(record.id);

            //   - Get the page index from PageDirectory.
            int pageIndex = PageDirectory[hashed_id];

            // If it's not in PageDirectory, define a new page using nextFreePage.
            if (pageIndex == -1) {
                pageIndex = nextFreePage;
                PageDirectory[hashed_id] = pageIndex;
                nextFreePage++;
            }

            //   - Insert the record into the appropriate page in the index file using addRecordToIndex() function.
            // Load up the index file
            fstream indexFile(fileName, ios::binary | ios::in | ios::out);

            // Find where the page is were looking for in the index file
            indexFile.seekg(pageIndex * Page_SIZE, ios::beg);

            // Read that page
            Page page;
            page.read_from_data_file(indexFile);

            // Add the record to that page
            addRecordToIndex(pageIndex, page, record);

            indexFile.close();
        }
        cout << nextFreePage << endl;
        // Close the CSV file
        csvFile.close();
    }

    // Function to search for a record by ID in the hash index
    void findAndPrintEmployee(int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // TODO:
        //  - Compute hash value for the given ID using compute_hash_value() function
        int hashed_id = compute_hash_value(id);

        //  - Search for the record in the page corresponding to the hash value
        int pageIndex = PageDirectory[hashed_id];

        // Go while pages exist
        while (pageIndex != -1) {
            // Retrieve the page we need from the indexFile
            indexFile.seekg(pageIndex * Page_SIZE, ios::beg);
            Page page;
            page.read_from_data_file(indexFile);

            // Search this page for records where the id matches the id given
            for (const auto& record : page.records) {
                if (record.id == id) {
                    record.print();
                    indexFile.close();
                    return;
                }
            }

            // If not found in this page, see if overflow exists and it will continue to check pages until no more overflow pages exist
            pageIndex = page.overflowPointerIndex;
        }


        cout << "Emp not found with id" << id << endl;

        // Close the index file
        indexFile.close();
    }
};

