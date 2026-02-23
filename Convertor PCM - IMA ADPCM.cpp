#include <iostream>
#include <fstream>
#include <cstring>
using namespace std;

///CONVERTESTE FISIERE WAV PCM LA FISIERE WAV IMA ADPCP ȘI INVERS
///EXACT DUPA MODELUL UNUI MP3-PLAYER CHINEZESC VECHI (REVERSE ENGINEERING)

///CE FACE? 8/16/24/32 bit PCM (n channels) (8000 Hz) -> 16-bit PCM (MONO) <-> 4-bit IMA ADPCM
///DE FACUT: Interpolare (conversie de la orice rata de esantionare la 8000 Hz)

typedef unsigned char Byte;
typedef unsigned int uint;

typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

struct Time{
    uint32_t h, m, s;

    void FromSeconds(uint32_t seconds){
        s = seconds % 60; seconds /= 60;
        m = seconds % 60; seconds /= 60;
        h = seconds;
    }
    void Print(){
        if(h < 10){ cout<<'0'; } cout<<h<<":";
        if(m < 10){ cout<<'0'; } cout<<m<<":";
        if(s < 10){ cout<<'0'; } cout<<s;
    }
};

const uint32_t RIFF = 'R' + ('I'<<8) + ('F'<<16) + ('F'<<24);
const uint32_t WAVE = 'W' + ('A'<<8) + ('V'<<16) + ('E'<<24);
const uint32_t fmt  = 'f' + ('m'<<8) + ('t'<<16) + (' '<<24);
const uint32_t Data = 'd' + ('a'<<8) + ('t'<<16) + ('a'<<24);

const int StepTab[89] = {
        7,     8,     9,    10,    11,    12,    13,    14,
       16,    17,    19,    21,    23,    25,    28,    31,
       34,    37,    41,    45,    50,    55,    60,    66,
       73,    80,    88,    97,   107,   118,   130,   143,
      157,   173,   190,   209,   230,   253,   279,   307,
      337,   371,   408,   449,   494,   544,   598,   658,
      724,   796,   876,   963,  1060,  1166,  1282,  1411,
     1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
     3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
     7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767 };

const int IndexTab[16] = { -1, -1, -1, -1, 2, 4, 6, 8,
                           -1, -1, -1, -1, 2, 4, 6, 8 };

const int fileEntrySize = 79;
const Byte fileEntry[fileEntrySize] = {
    'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',
    0x14,0,0,0,0x11,0,1,0,0x40,0x1F,0,0,0xA0,0x0F,0,0,0,2,4,0,
    2,0,0xF9,0x03,'f','a','c','t',4,0,0,0,0xDB,0xBA,9,0,
    'L','I','S','T',0xBC,1,0,0,'I','N','F','O','I','S','F','T',
    0x0A,0,0,0,'A','C','T','I','O','N','S'
};

class Wave{
private:
    uint16_t format, noOfChannels, blockSize, bitsPerSample,
             samplesPerBlock;
    uint32_t sampleRate, bytesPerSecond, factNumber;

    Time duration;
    int16_t* samples = NULL; uint32_t noOfSamples;
    Byte* fileBuffer = NULL; uint32_t fileSize;

    void ClearMem(){
        if(samples){ delete[] samples; }
        if(fileBuffer){ delete[] fileBuffer; }
    }
    void Exit(char errorMessage[]){
        cout<<'\n'<<errorMessage<<'\n';
        ClearMem(); exit(1);
    }

    void LoadFileBuffer(char fileName[]){
        ///Deschid fisierul
        ifstream fin(fileName, ios::binary);
        if(!fin){ Exit("File not found!"); }
        ///Aflu dimensiunea lui
        fin.seekg(0, ios::end);
        fileSize = fin.tellg();
        fin.seekg(0);
        ///Il incarc in buffer
        fileBuffer = new Byte[fileSize];
        fin.read((char*)fileBuffer, fileSize);
        ///Inchid fisierul
        fin.close();
    }
    bool isWaveHeader(){
        if(fileSize < 12){ return false; }
        return *((uint32_t*)fileBuffer) == RIFF &&
               *((uint32_t*)(fileBuffer + 8)) == WAVE;
    }
    void LoadParams(){
        Byte* p = FindChunk("fmt ");
        uint32_t formatSize = *((uint32_t*)(p + 4));
        if(formatSize < 0x10){ Exit("fmt  chunk too small!"); }

        format = *((uint16_t*)(p + 8));
        if(format != 0 && format != WAV_PCM && format != WAV_IMA_ADPCM){
            Exit("Wave format is not PCM or IMA ADPCM!");
        }
        if(format == WAV_IMA_ADPCM && formatSize < 0x14){
            Exit("fmt  chunk too small!");
        }

        noOfChannels = *((uint16_t*)(p + 10));
        sampleRate = *((uint32_t*)(p + 12));
        bytesPerSecond = *((uint32_t*)(p + 16));
        blockSize = *((uint16_t*)(p + 20));
        bitsPerSample = *((uint16_t*)(p + 22));

        if((format == WAV_PCM || !format) && bitsPerSample != 8 &&
           bitsPerSample != 16 && bitsPerSample != 24 &&
           bitsPerSample != 32){
            Exit("Only 8, 16, 24 or 32 bits / sample supported for PCM!");
           }
        else if(format == WAV_IMA_ADPCM){
            if(bitsPerSample != 4){
                Exit("Only 4 bits / sample supported for IMA_ADPCM!");
            }
            if(noOfChannels != 1){
                Exit("Only 1 channel supported for IMA_ADPCM!");
            }
        }

        if(format == WAV_IMA_ADPCM){
            samplesPerBlock = *((uint16_t*)(p + 26));

            p = FindChunk("fact");
            if(*((uint32_t*)(p + 4)) < 4){
                Exit("Fact chunk too short!");
            }
            factNumber = *((uint32_t*)(p + 8));
        }
    }

    Byte* FindChunk(char tagStr[5]){
        uint32_t tag = tagStr[0] + (tagStr[1] << 8) +
                       (tagStr[2] << 16) + (tagStr[3] << 24);

        Byte* p = fileBuffer + 12;
        while(fileBuffer+fileSize - p >= 8 && *((uint32_t*)p) != tag){
            uint32_t chunkSize = *((uint32_t*)(p + 4));
            if(chunkSize%2){ chunkSize++; }

            p += 8 + chunkSize;
        }
        if(fileBuffer+fileSize - p < 8){
            char msg[22]; strcpy(msg, tagStr);
            strcat(msg, " chunk not found!");
            Exit(msg);
        }
        return p;
    }
    void LoadData(){
        Byte* p = FindChunk("data");

        uint32_t dataBytes = *((uint32_t*)(p + 4));
        if(fileBuffer+fileSize - (p + 8) < dataBytes){
            dataBytes = fileBuffer+fileSize - (p + 8);
        }

        if(format == WAV_PCM || !format){
            uint16_t bytesPerSample = bitsPerSample >> 3;
            noOfSamples = dataBytes / bytesPerSample / noOfChannels;

            samples = new int16_t[noOfSamples];
            LoadPCMSamples(p + 8);
        }
        else if(format == WAV_IMA_ADPCM){
            uint32_t noOfBlocks = dataBytes / blockSize;
            noOfSamples = samplesPerBlock * noOfBlocks;

            samples = new int16_t[noOfSamples];
            LoadIMAADPCMSamples(p + 8);
        }
        duration.FromSeconds(noOfSamples / sampleRate);
    }

    void LoadPCMSamples(Byte* dataP){
        Byte* pD = dataP;
        int16_t* pS = samples;
		
		///Converteste la esantioane de 16 biti
		///Face media aritmetica intre valorile canalelor pentru a
		///obtine mono

        if(bitsPerSample == 8){
            while(pS - samples < noOfSamples){
                int16_t avgSamp = 0;
                for(int32_t k = noOfChannels; k--;){
                    int16_t samp = *pD;
                    samp -= 0x80;
                    samp <<= 8;

                    avgSamp += samp; pD++;
                }
                avgSamp /= noOfChannels;
                *pS = avgSamp; pS++;
            }
        }
        else if(bitsPerSample == 16){
            while(pS - samples < noOfSamples){
                int32_t avgSamp = 0;
                for(int32_t k = noOfChannels; k--;){
                    int16_t samp = *((int16_t*)pD);
                    avgSamp += samp; pD += 2;
                }
                avgSamp /= noOfChannels;
                *pS = avgSamp; pS++;
            }
        }
        else if(bitsPerSample == 24){
            while(pS - samples < noOfSamples){
                int32_t avgSamp = 0;
                for(int32_t k = noOfChannels; k--;){
                    int32_t samp = *((int32_t*)pD);
                    samp &= 0x00FFFFFF;
                    if(samp & 0x00800000){
                        samp |= 0xFF000000;
                    }
                    samp >>= 8;

                    avgSamp += samp; pD += 3;
                }
                avgSamp /= noOfChannels;
                *pS = avgSamp; pS++;
            }
        }
        else if(bitsPerSample == 32){
            while(pS - samples < noOfSamples){
                int64_t avgSamp = 0;
                for(int32_t k = noOfChannels; k--;){
                    int32_t samp = *((int32_t*)pD);
                    samp >>= 16;

                    avgSamp += samp; pD += 4;
                }
                avgSamp /= noOfChannels;
                *pS = avgSamp; pS++;
            }
        }
    }
    void LoadIMAADPCMSamples(Byte* dataP){
        Byte* pD = dataP;
        int16_t* pS = samples;

        ///Parametri Codificaror

        int Index = 0, PredSamp;

        ///Block-urile IMA-ADPCM

        while(pS - samples < noOfSamples){ //mai avem esantioane
            int16_t Samp0 = *((int16_t*)pD); pD += 2;
            Index = *pD; pD++; pD++; //inca un byte - rezervat

            *pS = Samp0; pS++;
            PredSamp = Samp0;

            int paritateEsantion = 0;

            while((pD - dataP) % blockSize){ //mai avem din blocul curent
                Byte Code;
                if(paritateEsantion == 0){
                    Code = *pD & 0x0F;
                }
                else{
                    Code = *pD >> 4; pD++;
                }
                paritateEsantion = 1 - paritateEsantion;

                int Delta = 0;
                if(Code & 4){
                    Delta += StepTab[Index];
                }
                if(Code & 2){
                    Delta += (StepTab[Index] >> 1);
                }
                if(Code & 1){
                    Delta += (StepTab[Index] >> 2);
                }
                Delta += (StepTab[Index] >> 3);
                if(Code & 8){
                    Delta = -Delta;
                }
                PredSamp += Delta;

                if(PredSamp > 32767){ PredSamp = 32767; }
                if(PredSamp < -32768){ PredSamp = -32768; }

                *pS = PredSamp; pS++;

                Index += IndexTab[Code];

                if(Index > 88){ Index = 88; }
                if(Index < 0){ Index = 0; }
            }
        }
    }

    void SetFileSize(){ //Dupa modelul acelui MP3-Player
        samplesPerBlock = 1017;
        blockSize = 512;

        uint noOfBlocks = noOfSamples / samplesPerBlock;
        uint sizeOfBlocks = noOfBlocks * blockSize;
        fileSize = blockSize + sizeOfBlocks;
    }

    void SaveFile(char fileName[]){
        ofstream fout(fileName, ios::binary);
        fout.write((char*)fileBuffer, fileSize);
        fout.close();
    }

public:
    enum Formats{ WAV_PCM = 1, WAV_IMA_ADPCM = 0x11 };

    Wave(){}
    Wave(char fileName[]){ Load(fileName); }

    ~Wave(){ ClearMem(); }

    uint16_t getFormat(){ return format; }
    uint16_t getBitsPerSample(){ return bitsPerSample; }

    void Load(char fileName[]){
        LoadFileBuffer(fileName);

        if(!isWaveHeader()){ Exit("Wave header not found!"); }
        LoadParams();
        LoadData();

        delete[] fileBuffer; fileBuffer = NULL;
    }

    void DisplayInfo(){
        cout<<"\nFile information:\n\n";

        cout<<"Format: "<<bitsPerSample<<"-bit ";
        if(format == WAV_PCM || !format){ cout<<"PCM"; }
        if(format == WAV_IMA_ADPCM){ cout<<"IMA-ADPCM"; }
        cout<<" wave file\n";

        if(noOfChannels == 1){ cout<<"Mono\n"; }
        else if(noOfChannels == 2){ cout<<"Stereo\n"; }
        else{ cout<<noOfChannels<<" channels\n"; }

        cout<<"Sample rate: "<<sampleRate<<" Hz\n";
        cout<<"Duration: "; duration.Print();

        cout<<"\n\nInteresting info\n\nBytes / second: ";
        cout<<bytesPerSecond<<'\n';
        cout<<"Block size: "<<blockSize<<'\n';
        if(format == WAV_IMA_ADPCM){
            cout<<"Samples / block: "<<samplesPerBlock<<'\n';
            cout<<"Fact: "<<factNumber;
			if(factNumber != noOfSamples){
                cout<<" (here not equal to the audio duration in samples)";
            }
            cout<<'\n';
        }
        cout<<'\n';
    }

    void SavePCM(char fileName[]){
        fileSize = 44 + 2*noOfSamples;
        fileBuffer = new Byte[fileSize];

        *((uint32_t*)fileBuffer) = RIFF;
        *((uint32_t*)(fileBuffer + 4)) = fileSize - 8;
        *((uint32_t*)(fileBuffer + 8)) = WAVE;

        *((uint32_t*)(fileBuffer + 12)) = fmt;
        *((uint32_t*)(fileBuffer + 16)) = 0x10;
        *((uint16_t*)(fileBuffer + 20)) = 1;
        *((uint16_t*)(fileBuffer + 22)) = 1;
        *((uint32_t*)(fileBuffer + 24)) = sampleRate;
        *((uint32_t*)(fileBuffer + 28)) = 2*sampleRate;
        *((uint16_t*)(fileBuffer + 32)) = 2;
        *((uint16_t*)(fileBuffer + 34)) = 16;

        *((uint32_t*)(fileBuffer + 36)) = Data;
        *((uint32_t*)(fileBuffer + 40)) = fileSize - 44;

        Byte* pD = fileBuffer + 44;
        int16_t* pS = samples;

        while(pS - samples < noOfSamples){
            *((int16_t*)pD) = *pS;
            pD += 2; pS++;
        }

        SaveFile(fileName);
        delete[] fileBuffer; fileBuffer = NULL;
    }

    void SaveIMAADPCM(char fileName[]){
        SetFileSize();
        fileBuffer = new Byte[fileSize];

        ///HEADER-UL WAV SPECIFIC RESPECTIVULUI MP3-Player
		///(Nu respecta intocmai specificatia formatului)

        for(int i = 0; i < fileEntrySize; i++){
            fileBuffer[i] = fileEntry[i];
        }
        for(int i = fileEntrySize; i < 0x1F8; i++){
            fileBuffer[i] = 0;
        }
        fileBuffer[0x100] = 0x81;
        *((uint32_t*)(fileBuffer + 0x1F8)) = Data;
        *((uint*)(fileBuffer + 4)) = fileSize; //Aici nu respecta conventia, ar trebui sa scada 8.
        *((uint*)(fileBuffer + 0x1FC)) = fileSize - 0x200; //Aici respecta.

        ///Parametri Codificaror

        int Index = 0, PredSamp;

        ///Block-urile IMA-ADPCM

        short int* audioP = samples;

        Byte* fileP = fileBuffer + 0x200;
        while(fileP - fileBuffer < fileSize){ //mai avem din fisier
            //Header-ul blocului
            *((short int*)fileP) = *audioP; PredSamp = *audioP;
            fileP += 2;
            audioP++; //Urmatorul esantion

            *fileP = (Byte)Index; fileP++;
            *fileP = 0; fileP++;

            int paritateEsantion = 0; //ca sa stim unde in byte punem codul de 4 biti

            //Restul informatiei din bloc
            while((fileP - fileBuffer) % 0x200){ //mai avem din bloc
                //Diferenta dintre esantionul curent si
                //ce va fi decodificat la redare din esantionul anterior.
                int Diff = *audioP - PredSamp;

                //Bitul de semn ("Code" e semn + magnitudine;
                //magnitudine e in virgula fixa)
                //
                //Code = sm.mm (s = bit de semn; m = bit din magnitudine;
                //. = virgula).
                Byte Code = 0;
                if(Diff < 0){
                    Code = 8;
                    Diff = -Diff;
                }

                //"Code" va inregistra cat de mare e "Diff"
                //raportat la dimensiunea pasului curent
                //(StepTab[Index]).
                //
                //Chiar face impartirea, ca pe hartie, dar
                //cu comparatii, scaderi si deplasari pe biti
                //(pt. procesoarele simple, fara float).
                //E si foarte rapid.
                //
                //Singura diferenta la impartire e ca,
                //in loc sa adauge un 0 la rest dupa o scadere,
                //ia o cifra (tot din dreapta, practic imparte la 2 - baza)
                //din impartitor => raportul e la fel.
                if(Diff >= StepTab[Index]){
                    Code |= 4;
                    Diff -= StepTab[Index];
                }
                if(Diff >= (StepTab[Index] >> 1)){
                    Code |= 2;
                    Diff = Diff - (StepTab[Index] >> 1);
                }
                if(Diff >= (StepTab[Index] >> 2)){
                    Code |= 1;
                }

                //Punem codul in bloc
                if(paritateEsantion == 0){
                    *fileP = Code;
                }
                else{
                    *fileP |= (Code << 4); fileP++;
                }

                //Vedem ce se decodifica din ceea ce tocmai
                //am codificat (folosit pentru diferenta urmatorului
                //esantion)
                //
                //Se inverseaza operatia anterioara (impartirea)
                //=> inmultire (la fel, cu deplasari pe biti si adunari)
                //
                //Se tine cont de repr. in virgula fixa (vezi la impartire,
                //mai sus)
                //
                //Se aduna si un "bias", sau o marja de eroare. Ajuta la:
                //1) Corectarea erorii produse de impartirea trunchiata
                //(la doar 2 zecimale).
                //2) Da un impuls pentru semnale audio foarte mici
                //(eroare corectata pentru urmatoarele esantioane, un fel de "dither")
                //Daca nu ar face asta, la semnalele foarte slabe
                //doar ar ramane pe loc (s-ar pierde sunetul), dar
                //"bias-ul" il "resusciteaza".
                int Delta = 0;
                if(Code & 4){
                    Delta += StepTab[Index];
                }
                if(Code & 2){
                    Delta += (StepTab[Index] >> 1);
                }
                if(Code & 1){
                    Delta += (StepTab[Index] >> 2);
                }
                Delta += (StepTab[Index] >> 3);
                //Verificam bitul de semn
                if(Code & 8){
                    Delta = -Delta;
                }
                PredSamp += Delta;
                if(PredSamp > 32767){ PredSamp = 32767; }
                if(PredSamp < -32768){ PredSamp = -32768; }

                //Ajustam marimea pasului (prin indice la tabel)
                //
                //Daca "Code" (raportul) e mai mic ca 1
                //(luand in seama virgula)
                //=> diferenta e mai mica decat pasul
                //=> putem micsora usor pasul.
                //
                //Daca e >= 1 => diferenta >= pasul
                //=> marim rapid (daca raportul e mai mare, atunci si mai rapid)
                //pasul.
				
                Index += IndexTab[Code];

                if(Index > 88){ Index = 88; }
                if(Index < 0){ Index = 0; }

                audioP++; //Urmatorul esantion
                paritateEsantion = 1 - paritateEsantion; //se schimba paritatea
            }
        }

        SaveFile(fileName);
        delete[] fileBuffer; fileBuffer = NULL;
    }
};

int main(){
    ///system("color EC");

    cout<<"CONVERTOR FISIERE WAV\n";
    cout<<"PCM (8000 Hz) <-> IMA-ADPCM\n\n";

	///DE FACUT: Sa accepte orice rata de esantionare (prin conversie
	///la 8000 Hz (interpolare))

    cout<<"Calea fisierului de convertit:\n\n";
    char fileName[1202];
    cin>>fileName[0]; cin.getline(fileName+1, 1200);
    Wave wav(fileName); wav.DisplayInfo();

    char optiune;
    enum optiuni{ adpcm = 'A', pcm = 'P' };
    cout<<"Convertire la:\n\n";

    cout<<"A) 4-bit IMA-ADPCM wave file\n";
    cout<<"P) 16-bit PCM wave file\n\n";

    cin>>optiune;
    if(optiune > 'Z'){ optiune += 'A' - 'a'; } //Daca litera introdusa e mica, o fac mare

    cout<<"\nCalea fisierului final:\n\n";
    cin>>fileName[0]; cin.getline(fileName+1, 1200);

    if(optiune == pcm){
        wav.SavePCM(fileName);
    }
    else{
        wav.SaveIMAADPCM(fileName);
    }
    return 0;
}
